
#ifndef PAHO_EXPORT_H
#define PAHO_EXPORT_H

#ifdef PAHO_STATIC_DEFINE
#  define PAHO_EXPORT
#  define PAHO_NO_EXPORT
#else
#  ifndef PAHO_EXPORT
#    ifdef Paho_EXPORTS
        /* We are building this library */
#      define PAHO_EXPORT 
#    else
        /* We are using this library */
#      define PAHO_EXPORT 
#    endif
#  endif

#  ifndef PAHO_NO_EXPORT
#    define PAHO_NO_EXPORT 
#  endif
#endif

#ifndef PAHO_DEPRECATED
#  define PAHO_DEPRECATED __declspec(deprecated)
#endif

#ifndef PAHO_DEPRECATED_EXPORT
#  define PAHO_DEPRECATED_EXPORT PAHO_EXPORT PAHO_DEPRECATED
#endif

#ifndef PAHO_DEPRECATED_NO_EXPORT
#  define PAHO_DEPRECATED_NO_EXPORT PAHO_NO_EXPORT PAHO_DEPRECATED
#endif

#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef PAHO_NO_DEPRECATED
#    define PAHO_NO_DEPRECATED
#  endif
#endif

#endif /* PAHO_EXPORT_H */
