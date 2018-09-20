// Copyright (C) 2017-2018 Jonathan Müller <jonathanmueller.dev@gmail.com>
// This file is subject to the license terms in the LICENSE file
// found in the top-level directory of this distribution.

#ifndef CPPAST_LIBCLANG_PARSER_HPP_INCLUDED
#define CPPAST_LIBCLANG_PARSER_HPP_INCLUDED

#include <stdexcept>

#include <cppast/parser.hpp>

namespace cppast
{
class libclang_compile_config;
class libclang_compilation_database;

namespace detail
{
    struct libclang_compile_config_access
    {
        static const std::string& clang_binary(const libclang_compile_config& config);

        static int clang_version(const libclang_compile_config& config);

        static const std::vector<std::string>& flags(const libclang_compile_config& config);

        static bool write_preprocessed(const libclang_compile_config& config);

        static bool fast_preprocessing(const libclang_compile_config& config);

        static bool remove_comments_in_macro(const libclang_compile_config& config);
    };

    void for_each_file(const libclang_compilation_database& database, void* user_data,
                       void (*callback)(void*, std::string));
} // namespace detail

/// The exception thrown when a fatal parse error occurs.
class libclang_error final : public std::runtime_error
{
public:
    /// \effects Creates it with a message.
    libclang_error(std::string msg) : std::runtime_error(std::move(msg)) {}
};

/// A compilation database.
///
/// This represents a `compile_commands.json` file,
/// which stores all the commands needed to compile a set of files.
/// It can be generated by CMake using the `CMAKE_EXPORT_COMPILE_COMMANDS` option.
class libclang_compilation_database
{
public:
    /// \effects Creates it giving the directory where the `compile_commands.json` file is located.
    /// \throws `libclang_error` if the database could not be loaded or found.
    libclang_compilation_database(const std::string& build_directory);

    libclang_compilation_database(libclang_compilation_database&& other)
    : database_(other.database_)
    {
        other.database_ = nullptr;
    }

    ~libclang_compilation_database();

    libclang_compilation_database& operator=(libclang_compilation_database&& other)
    {
        libclang_compilation_database tmp(std::move(other));
        std::swap(tmp.database_, database_);
        return *this;
    }

    /// \returns Whether or not the database contains information about the given file.
    /// \group has_config
    bool has_config(const char* file_name) const;

    /// \group has_config
    bool has_config(const std::string& file_name) const
    {
        return has_config(file_name.c_str());
    }

private:
    using database = void*;
    database database_;

    friend libclang_compile_config;
    friend void detail::for_each_file(const libclang_compilation_database& database,
                                      void* user_data, void (*callback)(void*, std::string));
};

/// Compilation config for the [cppast::libclang_parser]().
class libclang_compile_config final : public compile_config
{
public:
    /// Creates the default configuration.
    ///
    /// \effects It will set the clang binary determined by the build system,
    /// as well as the libclang system include directory determined by the build system.
    /// It will also define `__cppast__` with the value `"libclang"` as well as `__cppast_major__`
    /// and `__cppast_minor__`.
    libclang_compile_config();

    /// Creates the configuration stored in the database.
    ///
    /// \effects It will use the options found in the database for the specified file.
    /// This does not necessarily need to match the file that is going to be parsed,
    /// but it should.
    /// It will also add the default configuration options.
    /// \notes Header files are not included in the compilation database,
    /// you need to pass in the file name of the corresponding source file,
    /// if you want to parse one.
    /// \notes It will only consider options you could also set by the other functions.
    /// \notes The file key will include the specified directory in the JSON, if it is not a full
    /// path.
    libclang_compile_config(const libclang_compilation_database& database, const std::string& file);

    libclang_compile_config(const libclang_compile_config& other) = default;
    libclang_compile_config& operator=(const libclang_compile_config& other) = default;

    /// \effects Sets the path to the location of the `clang++` binary and the version of that
    /// binary. \notes It will be used for preprocessing.
    void set_clang_binary(std::string binary, int major, int minor, int patch)
    {
        clang_binary_  = std::move(binary);
        clang_version_ = major * 10000 + minor * 100 + patch;
    }

    /// \effects Sets whether or not the preprocessed file will be written out.
    /// Default value is `false`.
    void write_preprocessed(bool b) noexcept
    {
        write_preprocessed_ = b;
    }

    /// \effects Sets whether or not the fast preprocessor is enabled.
    /// Default value is `false`.
    /// \notes The fast preprocessor gets a list of all macros that are defined in the translation
    /// unit, then preprocesses it without resolving includes but manually defining the list of
    /// macros to ensure correctness. Later stages will use the includes again. This hack breaks if
    /// you define the same macro multiple times in the file being parsed (headers don't matter) or
    /// you rely on the order of macro directives. \notes If this option is `true`, the full file
    /// name of include directives is not available, just the name as written in the source code.
    void fast_preprocessing(bool b) noexcept
    {
        fast_preprocessing_ = b;
    }

    /// \effects Sets whether or not documentation comments generated by macros are removed.
    /// Default value is `false`.
    /// \notes If this leads to an error due to preprocessing and comments, you have to enable it.
    /// \notes If this is `true`, `clang` will be invoked with `-CC`, otherwise `-C`.
    void remove_comments_in_macro(bool b) noexcept
    {
        remove_comments_in_macro_ = b;
    }

private:
    void do_set_flags(cpp_standard standard, compile_flags flags) override;

    void do_add_include_dir(std::string path) override;

    void do_add_macro_definition(std::string name, std::string definition) override;

    void do_remove_macro_definition(std::string name) override;

    const char* do_get_name() const noexcept override
    {
        return "libclang";
    }

    std::string clang_binary_;
    int         clang_version_;
    bool        write_preprocessed_ : 1;
    bool        fast_preprocessing_ : 1;
    bool        remove_comments_in_macro_ : 1;

    friend detail::libclang_compile_config_access;
};

/// Finds a configuration for a given file.
///
/// \returns If the database contains a configuration for the given file, returns that
/// configuration. Otherwise removes the file extension of the file and tries the same procedure for
/// common C++ header and source file extensions. \notes This function is intended to be used as the
/// basis for a `get_config` function of
/// [cppast::parse_files](standardese://cppast::parse_files_basic/).
type_safe::optional<libclang_compile_config> find_config_for(
    const libclang_compilation_database& database, std::string file_name);

/// A parser that uses libclang.
class libclang_parser final : public parser
{
public:
    using config = libclang_compile_config;

    /// \effects Creates a parser using the default logger.
    libclang_parser();

    /// \effects Creates a parser that will log error messages using the specified logger.
    explicit libclang_parser(type_safe::object_ref<const diagnostic_logger> logger);

    ~libclang_parser() noexcept override;

private:
    std::unique_ptr<cpp_file> do_parse(const cpp_entity_index& idx, std::string path,
                                       const compile_config& config) const override;

    struct impl;
    std::unique_ptr<impl> pimpl_;
};

/// Parses multiple files using a [cppast::libclang_parser]() and a compilation database.
///
/// \effects Invokes [cppast::parse_files](standardese://parse_files_basic/) passing it the parser
/// and file names, and a `get_config` function using [cppast::find_config_for]().
///
/// \throws [cppast::libclang_error]() if no configuration for a given file could be found in the
/// database.
///
/// \requires `FileParser` must use the libclang parser.
/// i.e. `FileParser::parser` must be an alias of [cppast::libclang_parser]().
template <class FileParser, class Range>
void parse_files(FileParser& parser, Range&& file_names,
                 const libclang_compilation_database& database)
{
    static_assert(std::is_same<typename FileParser::parser, libclang_parser>::value,
                  "must use the libclang parser");
    parse_files(parser, std::forward<Range>(file_names), [&](const std::string& file) {
        auto config = find_config_for(database, file);
        if (!config)
            throw libclang_error("unable to find configuration for file '" + file + "'");
        return config.value();
    });
}

/// Parses the files specified in a compilation database using a [cppast::libclang_parser]().
///
/// \effects For each file specified in a compilation database,
/// uses the `FileParser` to parse the file with the configuration specified in the database.
///
/// \requires `FileParser` must have the same requirements as for
/// [cppast::parse_files](standardese://parse_files_basic/). It must also use the libclang parser,
/// i.e. `FileParser::parser` must be an alias of [cppast::libclang_parser]().
template <class FileParser>
void parse_database(FileParser& parser, const libclang_compilation_database& database)
{
    static_assert(std::is_same<typename FileParser::parser, libclang_parser>::value,
                  "must use the libclang parser");
    struct data_t
    {
        FileParser&                          parser;
        const libclang_compilation_database& database;
    } data{parser, database};
    detail::for_each_file(database, &data, [](void* ptr, std::string file) {
        auto& data = *static_cast<data_t*>(ptr);

        libclang_compile_config config(data.database, file);
        data.parser.parse(std::move(file), std::move(config));
    });
}
} // namespace cppast

#endif // CPPAST_LIBCLANG_PARSER_HPP_INCLUDED
