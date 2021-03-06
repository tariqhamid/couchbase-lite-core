//
//  Logging.hh
//  LiteCore
//
//  Created by Jens Alfke on 10/16/16.
//  Copyright © 2016 Couchbase. All rights reserved.
//

#pragma once
#include "slice.hh"
#include "PlatformCompat.hh"
#include <atomic>
#include <string>
#include <stdarg.h>
#include <stdint.h>

/*
    This is a configurable console-logging facility that lets logging be turned on and off independently for various subsystems or areas of the code. It's used similarly to printf:
        Log(@"the value of foo is %d", foo);

    You can associate a log message with a particular subsystem or tag by defining a logging domain. In one source file, define the domain:
        DefineLogDomain(Foo);
    If you need to use the same domain in other source files, add the line
        UsingLogDomain(Foo);
    Now you can use the Foo domain for logging:
        LogTo(Foo, @"the value of foo is %d", foo);
 
    By default, logging is compiled in but disabled at runtime.

    To enable logging in general, set the user default 'Log' to 'YES'. You can do this persistently using the 'defaults write' command; but it's very convenient during development to use the Arguments tab in the Xcode Executable Info panel. Just add a new entry to the arguments list, of the form "-Log YES". Now you can check and uncheck that as desired; the change will take effect when relaunching.

    Once logging is enabled, you can turn on individual domains. For any domain "Foo", to enable output from calls of the form LogTo(Foo, @"..."), set the user default 'LogFoo' to 'YES', just as above.
 
    You can use LogVerbose() and LogDebug() for messages that add more detail but shouldn't be seen by default when the domain is enabled. To enable verbose mode for a domain, e.g. 'Foo', set the default 'LogFooVerbose' to YES. To enable both verbose and debug modes, set 'LogFooDebug' to YES.

    Warn() is a related function that _always_ logs, and prefixes the message with "WARNING:".
        Warn(@"Reactor coolant system has failed");
 
    Note: Logging is still present in release/nondebug builds. I've found this to be very useful in tracking down problems in the field, since I can tell a user how to turn on logging, and then get detailed logs back. To disable logging code from being compiled at all, define the preprocessor symbol _DISABLE_LOGGING (in your prefix header or target build settings.)
*/ 

namespace litecore {

enum class LogLevel : int8_t {
    Uninitialized = -1,
    Debug,
    Verbose,
    Info,
    Warning,
    Error,
    None
};


class LogDomain {
public:
    LogDomain(const char *name, LogLevel level =LogLevel::Uninitialized)
    :_name(name),
     _level(level),
     _next(sFirstDomain)
    {
        sFirstDomain = this;
    }

    const char* name() const                        {return _name;}
    LogLevel level()                                {return _level == LogLevel::Uninitialized
                                                                        ? initLevel() : _level;}
    void setLevel(LogLevel lvl)                     {_level = lvl;}

    bool willLog(LogLevel lv =LogLevel::Info) const {return _level <= lv;}
    explicit operator bool() const                  {return willLog();}

    void log(LogLevel level, const char *fmt, ...) __printflike(3, 4);
    void vlog(LogLevel level, const char *fmt, va_list);

    /** The minimum level of logging for all LogDomains; defaults to Warning.
        Changes only take effect if made before any logging occurs. */
    static LogLevel MinLevel;

    static void (*Callback)(const LogDomain&, LogLevel, const char *message);

    static LogDomain* named(const char *name);

private:
    LogLevel initLevel();
    
    LogLevel _level;
    const char* const _name;
    LogDomain* const _next;

    static LogDomain* sFirstDomain;
};


extern LogDomain DefaultLog;


#ifndef C4_TESTS
std::string _logSlice(fleece::slice);
#define logSlice(S) (_logSlice((S)).c_str())
#endif

#ifdef _MSC_VER
#define LogToAt(DOMAIN, LEVEL, FMT, ...) \
    {if (_usuallyFalse((DOMAIN).willLog(LogLevel::LEVEL))) \
        (DOMAIN).log(LogLevel::LEVEL, FMT, ##__VA_ARGS__);}

#define LogTo(DOMAIN, FMT, ...)         LogToAt(DOMAIN, Info, FMT, ##__VA_ARGS__)
#define LogVerbose(DOMAIN, FMT, ...)    LogToAt(DOMAIN, Verbose, FMT, ##__VA_ARGS__)

#define Debug(FMT, ...)                 LogToAt(DefaultLog, Debug,   FMT, ##__VA_ARGS__)
#define Log(FMT, ...)                   LogToAt(DefaultLog, Info,    FMT, ##__VA_ARGS__)
#define Warn(FMT, ...)                  LogToAt(DefaultLog, Warning, FMT, ##__VA_ARGS__)
#define WarnError(FMT, ...)             LogToAt(DefaultLog, Error,   FMT, ##__VA_ARGS__)
#else
#define LogToAt(DOMAIN, LEVEL, FMT, ARGS...) \
    ({if (_usuallyFalse((DOMAIN).willLog(LogLevel::LEVEL))) \
        (DOMAIN).log(LogLevel::LEVEL, FMT, ##ARGS);})

#define LogTo(DOMAIN, FMT, ARGS...)         LogToAt(DOMAIN, Info, FMT, ##ARGS)
#define LogVerbose(DOMAIN, FMT, ARGS...)    LogToAt(DOMAIN, Verbose, FMT, ##ARGS)

#define Debug(FMT, ARGS...)                 LogToAt(DefaultLog, Debug,   FMT, ##ARGS)
#define Log(FMT, ARGS...)                   LogToAt(DefaultLog, Info,    FMT, ##ARGS)
#define Warn(FMT, ARGS...)                  LogToAt(DefaultLog, Warning, FMT, ##ARGS)
#define WarnError(FMT, ARGS...)             LogToAt(DefaultLog, Error,   FMT, ##ARGS)
#endif


static inline bool WillLog(LogLevel lv)     {return DefaultLog.willLog(lv);}


// Debug(...) is stripped out of release builds
#if !DEBUG
    #undef Debug
    #ifdef _MSC_VER
        #define Debug(FMT, ...)
    #else
        #define Debug(FMT...)      ({ })
    #endif
#endif


    /** Mixin that adds log(), warn(), etc. methods. The messages these write will be prefixed
        with a description of the object; by default this is just the class and address, but
        you can customize it by overriding loggingIdentifier(). */
    class Logging {
    protected:
        Logging(LogDomain &domain)
        :_domain(domain)
        { }

        virtual ~Logging() { }

        /** Override this to return a string identifying this object. */
        virtual std::string loggingIdentifier() const;

#if DEBUG
        // In debug mode, use code that's inefficient but allows use of __printflike, so the
        // compiler can catch invalid format parameters.
        #define LOGBODY(LEVEL)  va_list args; \
                                va_start(args, format); \
                                _logv(LogLevel::LEVEL, format, args); \
                                va_end(args);
        inline void log(const char *format, ...) const __printflike(2, 3)        {LOGBODY(Info)}
        inline void warn(const char *format, ...) const __printflike(2, 3)       {LOGBODY(Warning)}
        inline void logError(const char *format, ...) const __printflike(2, 3)   {LOGBODY(Error)}
        inline void logVerbose(const char *format, ...) const __printflike(2, 3) {LOGBODY(Verbose)}
        inline void logDebug(const char *format, ...) const __printflike(2, 3)   {LOGBODY(Debug)}

#else
        // In release mode, generate efficient code (but it can't be type-checked.)
        template <LogLevel LEVEL =LogLevel::Info, class... ARGS>
        inline void log(const char *format, ARGS... args) const {
            if (_usuallyFalse(_domain.willLog(LEVEL)))
                _log(LEVEL, format, args...);
        }

        template <class... ARGS>
        inline void warn(const char *format, ARGS... args) const {
            log<LogLevel::Warning>(format, args...);
        }
        template <class... ARGS>
        inline void logError(const char *format, ARGS... args) const {
            log<LogLevel::Error>(format, args...);
        }
        template <class... ARGS>
        inline void logVerbose(const char *format, ARGS... args) const {
            log<LogLevel::Verbose>(format, args...);
        }

        inline void logDebug(const char *format, ...) const {
            // does nothing in a release build
        }
#endif
        bool willLog(LogLevel level =LogLevel::Info) const         {return _domain.willLog(level);}

        void _log(LogLevel level, const char *format, ...) const;// __printflike(3, 4);
        void _logv(LogLevel level, const char *format, va_list) const;
        LogDomain &_domain;
    };

}
