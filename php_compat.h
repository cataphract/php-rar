#include <php.h>

#if PHP_MAJOR_VERSION >= 8
# define TSRMLS_DC
# define TSRMLS_D
# define TSRMLS_CC
# define TSRMLS_C
# define TSRMLS_FETCH()
# define IS_CALLABLE_STRICT 0
# define zend_qsort zend_sort
# define ZV_TO_THIS_FOR_HANDLER(zv) (Z_OBJ_P(zv))
typedef zend_object handler_this_t;
#else 
# define ZV_TO_THIS_FOR_HANDLER(zv) (zv)
typedef zval handler_this_t;
#endif

typedef zend_object* rar_obj_ref;

#define rar_zval_add_ref(ppzv) zval_add_ref(*ppzv)

#define ZVAL_ALLOC_DUP(dst, src) \
    do { \
        dst = (zval*) emalloc(sizeof(zval)); \
        ZVAL_DUP(dst, src); \
    } while (0)

#define RAR_RETURN_STRINGL(s, l, duplicate) \
    do { \
        RETVAL_STRINGL(s, l); \
        if (duplicate == 0) { \
            efree(s); \
        } \
        return; \
    } while (0)

#define RAR_ZVAL_STRING(z, s, duplicate) \
    do { \
        ZVAL_STRING(z, s); \
        if (duplicate == 0) { \
            efree(s); \
        } \
    } while (0)

typedef size_t zpp_s_size_t;

#define MAKE_STD_ZVAL(zv_p) \
    do { \
        (zv_p) = emalloc(sizeof(zval)); \
        ZVAL_NULL(zv_p); \
    } while (0)
#define INIT_ZVAL(zv) ZVAL_UNDEF(&zv)

#define ZEND_ACC_FINAL_CLASS ZEND_ACC_FINAL
