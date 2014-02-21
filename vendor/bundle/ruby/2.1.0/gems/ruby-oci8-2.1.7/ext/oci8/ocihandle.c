/* -*- c-file-style: "ruby"; indent-tabs-mode: nil -*- */
/*
 * ocihandle.c
 *
 * Copyright (C) 2009-2013 Kubo Takehiro <kubo@jiubao.org>
 *
 * implement OCIHandle
 *
 */
#include "oci8.h"

#ifdef _MSC_VER
#define MAGIC_NUMBER 0xDEAFBEAFDEAFBEAFui64;
#else
#define MAGIC_NUMBER 0xDEAFBEAFDEAFBEAFull;
#endif

static long check_data_range(VALUE val, long min, long max, const char *type)
{
    long lval = NUM2LONG(val);
    if (lval < min || max < lval) {
        rb_raise(rb_eRangeError, "integer %ld too %s to convert to `%s'",
                 lval, lval < 0 ? "small" : "big", type);
    }
    return lval;
}


static oci8_base_vtable_t oci8_base_vtable = {
    NULL,
    NULL,
    sizeof(oci8_base_t),
};

static VALUE oci8_handle_initialize(VALUE self)
{
    rb_raise(rb_eNameError, "private method `new' called for %s:Class", rb_class2name(CLASS_OF(self)));
}

/*
 * Clears the object internal structure and its dependents.
 *
 * @since 2.0.0
 * @private
 */
static VALUE oci8_handle_free(VALUE self)
{
    oci8_base_t *base = DATA_PTR(self);

    oci8_base_free(base);
    return self;
}

static void oci8_handle_mark(oci8_base_t *base)
{
    if (base->vptr->mark != NULL)
        base->vptr->mark(base);
}

static void oci8_handle_cleanup(oci8_base_t *base)
{
    if (oci8_in_finalizer) {
        /* Do nothing when the program exits.
         * The first two words of memory addressed by VALUE datatype is
         * changed in finalizer. If a ruby function which access it such
         * as rb_obj_is_kind_of is called, it may cause SEGV.
         */
        return;
    }
    oci8_base_free(base);
    xfree(base);
}

static VALUE oci8_s_allocate(VALUE klass)
{
    oci8_base_t *base;
    const oci8_base_vtable_t *vptr;
    VALUE superklass;
    VALUE obj;

    superklass = klass;
    while (!RTEST(rb_ivar_defined(superklass, oci8_id_oci8_vtable))) {
        superklass = rb_class_superclass(superklass);
        if (superklass == rb_cObject)
            rb_raise(rb_eRuntimeError, "private method `new' called for %s:Class", rb_class2name(klass));
    }
    obj = rb_ivar_get(superklass, oci8_id_oci8_vtable);
    vptr = DATA_PTR(obj);

    base = xmalloc(vptr->size);
    memset(base, 0, vptr->size);

    obj = Data_Wrap_Struct(klass, oci8_handle_mark, oci8_handle_cleanup, base);
    base->self = obj;
    base->vptr = vptr;
    base->parent = NULL;
    base->next = base;
    base->prev = base;
    base->children = NULL;
    if (vptr->init != NULL) {
        vptr->init(base);
    }
    return obj;
}

enum datatype {
    DATATYPE_UB1,
    DATATYPE_UB2,
    DATATYPE_UB4,
    DATATYPE_UB8,
    DATATYPE_SB1,
    DATATYPE_SB2,
    DATATYPE_SB4,
    DATATYPE_SB8,
    DATATYPE_BOOLEAN,
    DATATYPE_STRING,
    DATATYPE_BINARY,
    DATATYPE_INTEGER,
    DATATYPE_ORADATE,
};

static VALUE attr_get_common(int argc, VALUE *argv, VALUE self, enum datatype datatype)
{
    oci8_base_t *base = DATA_PTR(self);
    VALUE attr_type;
    VALUE strict;
    VALUE args[6];
    union {
        ub1 ub1val;
        ub2 ub2val;
        ub4 ub4val;
        ub8 ub8val;
        sb1 sb1val;
        sb2 sb2val;
        sb4 sb4val;
        sb8 sb8val;
        boolean booleanval;
        char *charptr;
        ub1 *ub1ptr;
    } v;
    ub4 size = 0;
    sword rv;

    if (base->type == 0) {
        return Qnil;
    }

    v.ub8val = MAGIC_NUMBER;
    rb_scan_args(argc, argv, "11", &attr_type, &strict);
    if (argc == 1) {
        strict = Qtrue;
    }
    Check_Type(attr_type, T_FIXNUM);
    rv = OCIAttrGet(base->hp.ptr, base->type, &v, &size, FIX2INT(attr_type), oci8_errhp);
    if (!RTEST(strict)) {
        if (rv == OCI_ERROR && oci8_get_error_code(oci8_errhp) == 24328) {
			/* ignore ORA-24328: illegal attribute value */
            return Qnil;
        }
    }
    chker2(rv, base);
    switch (datatype) {
        OCINumber onum;
        static VALUE cOraDate = Qnil;
    case DATATYPE_UB1:
        return INT2FIX(v.ub1val);
    case DATATYPE_UB2:
        return INT2FIX(v.ub2val);
    case DATATYPE_UB4:
        return UINT2NUM(v.ub4val);
    case DATATYPE_UB8:
        return ULL2NUM(v.ub8val);
    case DATATYPE_SB1:
        return INT2FIX(v.sb1val);
    case DATATYPE_SB2:
        return INT2FIX(v.sb2val);
    case DATATYPE_SB4:
        return INT2NUM(v.sb4val);
    case DATATYPE_SB8:
        return LL2NUM(v.sb8val);
    case DATATYPE_BOOLEAN:
        return v.booleanval ? Qtrue : Qfalse;
    case DATATYPE_STRING:
        if (size == 0 && !RTEST(strict)) {
            return Qnil;
        }
        return rb_external_str_new_with_enc(v.charptr, size, oci8_encoding);
    case DATATYPE_BINARY:
        return rb_tainted_str_new(v.charptr, size);
    case DATATYPE_INTEGER:
        if (size > sizeof(onum.OCINumberPart) - 1) {
            rb_raise(rb_eRuntimeError, "Too long size %u", size);
        }
        memset(&onum, 0, sizeof(onum));
        onum.OCINumberPart[0] = size;
        memcpy(&onum.OCINumberPart[1], v.ub1ptr, size);
        return oci8_make_integer(&onum, oci8_errhp);
    case DATATYPE_ORADATE:
        if (NIL_P(cOraDate))
            cOraDate = rb_eval_string("OraDate");
        args[0] = INT2FIX((v.ub1ptr[0] - 100) * 100 + (v.ub1ptr[1] - 100));
        args[1] = INT2FIX(v.ub1ptr[2]);
        args[2] = INT2FIX(v.ub1ptr[3]);
        args[3] = INT2FIX(v.ub1ptr[4] - 1);
        args[4] = INT2FIX(v.ub1ptr[5] - 1);
        args[5] = INT2FIX(v.ub1ptr[6] - 1);
        return rb_class_new_instance(6, args, cOraDate);
    }
    return Qnil;
}

/*
 * call-seq:
 *   attr_get_ub1(attr_type, strict = true)
 *
 * Gets the value of an attribute as `ub1' datatype.
 *
 * @param [Fixnum] attr_type
 * @param [Boolean] strict If false, "ORA-24328: illegal attribute value" is ignored.
 * @return [Fixnum]
 *
 * @since 2.0.4
 * @private
 */
static VALUE attr_get_ub1(int argc, VALUE *argv, VALUE self)
{
    return attr_get_common(argc, argv, self, DATATYPE_UB1);
}

/*
 * call-seq:
 *   attr_get_ub2(attr_type, strict = true)
 *
 * Gets the value of an attribute as `ub2' datatype.
 *
 * @param [Fixnum] attr_type
 * @param [Boolean] strict If false, "ORA-24328: illegal attribute value" is ignored.
 * @return [Fixnum]
 *
 * @since 2.0.4
 * @private
 */
static VALUE attr_get_ub2(int argc, VALUE *argv, VALUE self)
{
    return attr_get_common(argc, argv, self, DATATYPE_UB2);
}

/*
 * call-seq:
 *   attr_get_ub4(attr_type, strict = true)
 *
 * Gets the value of an attribute as `ub4' datatype.
 *
 * @param [Fixnum] attr_type
 * @param [Boolean] strict If false, "ORA-24328: illegal attribute value" is ignored.
 * @return [Integer]
 *
 * @since 2.0.4
 * @private
 */
static VALUE attr_get_ub4(int argc, VALUE *argv, VALUE self)
{
    return attr_get_common(argc, argv, self, DATATYPE_UB4);
}

/*
 * call-seq:
 *   attr_get_ub8(attr_type, strict = true)
 *
 * Gets the value of an attribute as `ub8' datatype.
 *
 * @param [Fixnum] attr_type
 * @param [Boolean] strict If false, "ORA-24328: illegal attribute value" is ignored.
 * @return [Integer]
 *
 * @since 2.0.4
 * @private
 */
static VALUE attr_get_ub8(int argc, VALUE *argv, VALUE self)
{
    return attr_get_common(argc, argv, self, DATATYPE_UB8);
}

/*
 * call-seq:
 *   attr_get_sb1(attr_type, strict = true)
 *
 * Gets the value of an attribute as `sb1' datatype.
 *
 * @param [Fixnum] attr_type
 * @param [Boolean] strict If false, "ORA-24328: illegal attribute value" is ignored.
 * @return [Fixnum]
 *
 * @since 2.0.4
 * @private
 */
static VALUE attr_get_sb1(int argc, VALUE *argv, VALUE self)
{
    return attr_get_common(argc, argv, self, DATATYPE_SB1);
}

/*
 * call-seq:
 *   attr_get_sb2(attr_type, strict = true)
 *
 * Gets the value of an attribute as `sb2' datatype.
 *
 * @param [Fixnum] attr_type
 * @param [Boolean] strict If false, "ORA-24328: illegal attribute value" is ignored.
 * @return [Fixnum]
 *
 * @since 2.0.4
 * @private
 */
static VALUE attr_get_sb2(int argc, VALUE *argv, VALUE self)
{
    return attr_get_common(argc, argv, self, DATATYPE_SB2);
}

/*
 * call-seq:
 *   attr_get_sb4(attr_type, strict = true)
 *
 * Gets the value of an attribute as `sb4' datatype.
 *
 * @param [Fixnum] attr_type
 * @param [Boolean] strict If false, "ORA-24328: illegal attribute value" is ignored.
 * @return [Integer]
 *
 * @since 2.0.4
 * @private
 */
static VALUE attr_get_sb4(int argc, VALUE *argv, VALUE self)
{
    return attr_get_common(argc, argv, self, DATATYPE_SB4);
}

/*
 * call-seq:
 *   attr_get_sb8(attr_type, strict = true)
 *
 * Gets the value of an attribute as `sb8' datatype.
 *
 * @param [Fixnum] attr_type
 * @param [Boolean] strict If false, "ORA-24328: illegal attribute value" is ignored.
 * @return [Integer]
 *
 * @since 2.0.4
 * @private
 */
static VALUE attr_get_sb8(int argc, VALUE *argv, VALUE self)
{
    return attr_get_common(argc, argv, self, DATATYPE_SB8);
}

/*
 * call-seq:
 *   attr_get_boolean(attr_type, strict = true)
 *
 * Gets the value of an attribute as `boolean' datatype.
 *
 * @param [Fixnum] attr_type
 * @param [Boolean] strict If false, "ORA-24328: illegal attribute value" is ignored.
 * @return [true of false]
 *
 * @since 2.0.4
 * @private
 */
static VALUE attr_get_boolean(int argc, VALUE *argv, VALUE self)
{
    return attr_get_common(argc, argv, self, DATATYPE_BOOLEAN);
}

/*
 * call-seq:
 *   attr_get_string(attr_type, strict = true)
 *
 * Gets the value of an attribute as `oratext *' datatype.
 * The return value is converted to Encoding.default_internal or
 * tagged with {OCI8.encoding} when the ruby version is 1.9.
 *
 * @note If the specified attr_type's datatype is not a
 *   pointer type, it causes a segmentation fault.
 *
 * @param [Fixnum] attr_type
 * @param [Boolean] strict If false, "ORA-24328: illegal attribute value" is ignored.
 * @return [String]
 *
 * @since 2.0.4
 * @private
 */
static VALUE attr_get_string(int argc, VALUE *argv, VALUE self)
{
    return attr_get_common(argc, argv, self, DATATYPE_STRING);
}

/*
 * call-seq:
 *   attr_get_binary(attr_type, strict = true)
 *
 * Gets the value of an attribute as `ub1 *' datatype.
 * The return value is tagged with ASCII-8BIT when the ruby version is 1.9.
 *
 * @note If the specified attr_type's datatype is not a
 *   pointer type, it causes a segmentation fault.
 *
 * @param [Fixnum] attr_type
 * @param [Boolean] strict If false, "ORA-24328: illegal attribute value" is ignored.
 * @return [String]
 *
 * @since 2.0.4
 * @private
 */
static VALUE attr_get_binary(int argc, VALUE *argv, VALUE self)
{
    return attr_get_common(argc, argv, self, DATATYPE_BINARY);
}

/*
 * call-seq:
 *   attr_get_integer(attr_type, strict = true)
 *
 * Gets the value of an attribute as `ub1 *' datatype.
 * The return value is converted to Integer from internal Oracle NUMBER format.
 *
 * @note If the specified attr_type's datatype is not a
 *   pointer type, it causes a segmentation fault.
 *
 * @param [Fixnum] attr_type
 * @param [Boolean] strict If false, "ORA-24328: illegal attribute value" is ignored.
 * @return [Fixnum]
 *
 * @since 2.0.4
 * @private
 */
static VALUE attr_get_integer(int argc, VALUE *argv, VALUE self)
{
    return attr_get_common(argc, argv, self, DATATYPE_INTEGER);
}

/*
 * call-seq:
 *   attr_get_oradate(attr_type, strict = true)
 *
 * Gets the value of an attribute as `ub1 *' datatype.
 * The return value is converted to OraDate.
 *
 * @note If the specified attr_type's datatype is not a
 *   pointer type, it causes a segmentation fault.
 *
 * @param [Fixnum] attr_type
 * @param [Boolean] strict If false, "ORA-24328: illegal attribute value" is ignored.
 * @return [OraDate]
 *
 * @since 2.0.4
 * @private
 */
static VALUE attr_get_oradate(int argc, VALUE *argv, VALUE self)
{
    return attr_get_common(argc, argv, self, DATATYPE_ORADATE);
}

/*
 * call-seq:
 *   attr_set_ub1(attr_type, attr_value)
 *
 * Sets the value of an attribute as `ub1' datatype.
 *
 * @note If the specified attr_type's datatype is a
 *   pointer type, it causes a segmentation fault.
 *
 * @param [Fixnum] attr_type
 * @param [Fixnum] attr_value
 * @return [self]
 *
 * @since 2.0.4
 * @private
 */
static VALUE attr_set_ub1(VALUE self, VALUE attr_type, VALUE val)
{
    oci8_base_t *base = DATA_PTR(self);
    ub1 value;

    /* validate arguments */
    Check_Type(attr_type, T_FIXNUM);
    value = (ub1)check_data_range(val, 0, UCHAR_MAX, "ub1");
    /* set attribute */
    chker2(OCIAttrSet(base->hp.ptr, base->type, &value, sizeof(value), FIX2INT(attr_type), oci8_errhp), base);
    return self;
}

/*
 * call-seq:
 *   attr_set_ub2(attr_type, attr_value)
 *
 * Sets the value of an attribute as `ub2' datatype.
 *
 * @note If the specified attr_type's datatype is a
 *   pointer type, it causes a segmentation fault.
 *
 * @param [Fixnum] attr_type
 * @param [Fixnum] attr_value
 * @return [self]
 *
 * @since 2.0.4
 * @private
 */
static VALUE attr_set_ub2(VALUE self, VALUE attr_type, VALUE val)
{
    oci8_base_t *base = DATA_PTR(self);
    ub2 value;

    /* validate arguments */
    Check_Type(attr_type, T_FIXNUM);
    value = (ub2)check_data_range(val, 0, USHRT_MAX, "ub2");
    /* set attribute */
    chker2(OCIAttrSet(base->hp.ptr, base->type, &value, sizeof(value), FIX2INT(attr_type), oci8_errhp), base);
    return self;
}

/*
 * call-seq:
 *   attr_set_ub4(attr_type, attr_value)
 *
 * Sets the value of an attribute as `ub4' datatype.
 *
 * @note If the specified attr_type's datatype is a
 *   pointer type, it causes a segmentation fault.
 *
 * @param [Fixnum] attr_type
 * @param [Integer] attr_value
 * @return [self]
 *
 * @since 2.0.4
 * @private
 */
static VALUE attr_set_ub4(VALUE self, VALUE attr_type, VALUE val)
{
    oci8_base_t *base = DATA_PTR(self);
    ub4 value;

    /* validate arguments */
    Check_Type(attr_type, T_FIXNUM);
    value = NUM2UINT(val);
    /* set attribute */
    chker2(OCIAttrSet(base->hp.ptr, base->type, &value, sizeof(value), FIX2INT(attr_type), oci8_errhp), base);
    return self;
}

/*
 * call-seq:
 *   attr_set_ub8(attr_type, attr_value)
 *
 * Sets the value of an attribute as `ub8' datatype.
 *
 * @note If the specified attr_type's datatype is a
 *   pointer type, it causes a segmentation fault.
 *
 * @param [Fixnum] attr_type
 * @param [Integer] attr_value
 * @return [self]
 *
 * @since 2.0.4
 * @private
 */
static VALUE attr_set_ub8(VALUE self, VALUE attr_type, VALUE val)
{
    oci8_base_t *base = DATA_PTR(self);
    ub8 value;

    /* validate arguments */
    Check_Type(attr_type, T_FIXNUM);
    value = NUM2ULL(val);
    /* set attribute */
    chker2(OCIAttrSet(base->hp.ptr, base->type, &value, sizeof(value), FIX2INT(attr_type), oci8_errhp), base);
    return self;
}

/*
 * call-seq:
 *   attr_set_sb1(attr_type, attr_value)
 *
 * Sets the value of an attribute as `sb1' datatype.
 *
 * @note If the specified attr_type's datatype is a
 *   pointer type, it causes a segmentation fault.
 *
 * @param [Fixnum] attr_type
 * @param [Fixnum] attr_value
 * @return [self]
 *
 * @since 2.0.4
 * @private
 */
static VALUE attr_set_sb1(VALUE self, VALUE attr_type, VALUE val)
{
    oci8_base_t *base = DATA_PTR(self);
    sb1 value;

    /* validate arguments */
    Check_Type(attr_type, T_FIXNUM);
    value = (sb1)check_data_range(val, CHAR_MIN, CHAR_MAX, "sb1");
    /* set attribute */
    chker2(OCIAttrSet(base->hp.ptr, base->type, &value, sizeof(value), FIX2INT(attr_type), oci8_errhp), base);
    return self;
}

/*
 * call-seq:
 *   attr_set_sb2(attr_type, attr_value)
 *
 * Sets the value of an attribute as `sb2' datatype.
 *
 * @note If the specified attr_type's datatype is a
 *  pointer type, it causes a segmentation fault.
 *
 * @param [Fixnum] attr_type
 * @param [Fixnum] attr_value
 * @return [self]
 *
 * @since 2.0.4
 * @private
 */
static VALUE attr_set_sb2(VALUE self, VALUE attr_type, VALUE val)
{
    oci8_base_t *base = DATA_PTR(self);
    sb2 value;

    /* validate arguments */
    Check_Type(attr_type, T_FIXNUM);
    value = (sb2)check_data_range(val, SHRT_MIN, SHRT_MAX, "sb2");
    /* set attribute */
    chker2(OCIAttrSet(base->hp.ptr, base->type, &value, sizeof(value), FIX2INT(attr_type), oci8_errhp), base);
    return self;
}

/*
 * call-seq:
 *   attr_set_sb4(attr_type, attr_value)
 *
 * Sets the value of an attribute as `sb4' datatype.
 *
 * @note If the specified attr_type's datatype is a
 *   pointer type, it causes a segmentation fault.
 *
 * @param [Fixnum] attr_type
 * @param [Integer] attr_value
 * @return [self]
 *
 * @since 2.0.4
 * @private
 */
static VALUE attr_set_sb4(VALUE self, VALUE attr_type, VALUE val)
{
    oci8_base_t *base = DATA_PTR(self);
    sb4 value;

    /* validate arguments */
    Check_Type(attr_type, T_FIXNUM);
    value = NUM2INT(val);
    /* set attribute */
    chker2(OCIAttrSet(base->hp.ptr, base->type, &value, sizeof(value), FIX2INT(attr_type), oci8_errhp), base);
    return self;
}

/*
 * call-seq:
 *   attr_set_sb8(attr_type, attr_value)
 *
 * Sets the value of an attribute as `sb8' datatype.
 *
 * @note If the specified attr_type's datatype is a
 *   pointer type, it causes a segmentation fault.
 *
 * @param [Fixnum] attr_type
 * @param [Integer] attr_value
 * @return [self]
 *
 * @since 2.0.4
 * @private
 */
static VALUE attr_set_sb8(VALUE self, VALUE attr_type, VALUE val)
{
    oci8_base_t *base = DATA_PTR(self);
    sb8 value;

    /* validate arguments */
    Check_Type(attr_type, T_FIXNUM);
    value = NUM2LL(val);
    /* set attribute */
    chker2(OCIAttrSet(base->hp.ptr, base->type, &value, sizeof(value), FIX2INT(attr_type), oci8_errhp), base);
    return self;
}

/*
 * call-seq:
 *   attr_set_boolean(attr_type, attr_value)
 *
 * Sets the value of an attribute as `boolean' datatype.
 *
 * @note If the specified attr_type's datatype is a
 *   pointer type, it causes a segmentation fault.
 *
 * @param [Fixnum] attr_type
 * @param [true or false] attr_value
 * @return [self]
 *
 * @since 2.0.4
 * @private
 */
static VALUE attr_set_boolean(VALUE self, VALUE attr_type, VALUE val)
{
    oci8_base_t *base = DATA_PTR(self);
    boolean value;

    /* validate arguments */
    Check_Type(attr_type, T_FIXNUM);
    value = RTEST(val) ? TRUE : FALSE;
    /* set attribute */
    chker2(OCIAttrSet(base->hp.ptr, base->type, &value, sizeof(value), FIX2INT(attr_type), oci8_errhp), base);
    return self;
}

/*
 * call-seq:
 *   attr_set_string(attr_type, attr_value)
 *
 * Sets the value of an attribute as `oratext *' datatype.
 * +attr_value+ is converted to {OCI8.encoding} before it is set
 * when the ruby version is 1.9.
 *
 * @param [Fixnum] attr_type
 * @param [String] attr_value
 * @return [self]
 *
 * @since 2.0.4
 * @private
 */
static VALUE attr_set_string(VALUE self, VALUE attr_type, VALUE val)
{
    oci8_base_t *base = DATA_PTR(self);

    /* validate arguments */
    Check_Type(attr_type, T_FIXNUM);
    OCI8SafeStringValue(val);
    /* set attribute */
    chker2(OCIAttrSet(base->hp.ptr, base->type, RSTRING_PTR(val), RSTRING_LEN(val), FIX2INT(attr_type), oci8_errhp), base);
    return self;
}

/*
 * call-seq:
 *   attr_set_binary(attr_type, attr_value)
 *
 * Sets the value of an attribute as `ub1 *' datatype.
 *
 * @param [Fixnum] attr_type
 * @param [String] attr_value
 * @return [self]
 *
 * @since 2.0.4
 * @private
 */
static VALUE attr_set_binary(VALUE self, VALUE attr_type, VALUE val)
{
    oci8_base_t *base = DATA_PTR(self);

    /* validate arguments */
    Check_Type(attr_type, T_FIXNUM);
    SafeStringValue(val);
    /* set attribute */
    chker2(OCIAttrSet(base->hp.ptr, base->type, RSTRING_PTR(val), RSTRING_LEN(val), FIX2INT(attr_type), oci8_errhp), base);
    return self;
}

/*
 * call-seq:
 *   attr_set_integer(attr_type, number)
 *
 * Sets the value of an attribute as `ub1 *' datatype.
 * +number+ is converted to internal Oracle NUMBER format before
 * it is set.
 *
 * @param [Fixnum] attr_type
 * @param [Numeric] number
 * @return [self]
 *
 * @since 2.0.4
 * @private
 */
static VALUE attr_set_integer(VALUE self, VALUE attr_type, VALUE val)
{
    oci8_base_t *base = DATA_PTR(self);
    OCINumber value;

    /* validate arguments */
    Check_Type(attr_type, T_FIXNUM);
    oci8_set_integer(&value, val, oci8_errhp);
    /* set attribute */
    chker2(OCIAttrSet(base->hp.ptr, base->type, &value, sizeof(value), FIX2INT(attr_type), oci8_errhp), base);
    return self;
}

void Init_oci8_handle(void)
{
    VALUE obj;

    /*
     * OCIHandle is the abstract base class of OCI handles and
     * OCI descriptors; opaque data types of Oracle Call Interface.
     * Don't use constants and methods defined in the class.
     *
     * @since 2.0.0
     */
    oci8_cOCIHandle = rb_define_class("OCIHandle", rb_cObject);
    rb_define_alloc_func(oci8_cOCIHandle, oci8_s_allocate);
    rb_define_method_nodoc(oci8_cOCIHandle, "initialize", oci8_handle_initialize, 0);
    rb_define_private_method(oci8_cOCIHandle, "free", oci8_handle_free, 0);
    obj = Data_Wrap_Struct(rb_cObject, 0, 0, &oci8_base_vtable);
    rb_ivar_set(oci8_cOCIHandle, oci8_id_oci8_vtable, obj);

    /* methods to get attributes */
    rb_define_private_method(oci8_cOCIHandle, "attr_get_ub1", attr_get_ub1, -1);
    rb_define_private_method(oci8_cOCIHandle, "attr_get_ub2", attr_get_ub2, -1);
    rb_define_private_method(oci8_cOCIHandle, "attr_get_ub4", attr_get_ub4, -1);
    rb_define_private_method(oci8_cOCIHandle, "attr_get_ub8", attr_get_ub8, -1);
    rb_define_private_method(oci8_cOCIHandle, "attr_get_sb1", attr_get_sb1, -1);
    rb_define_private_method(oci8_cOCIHandle, "attr_get_sb2", attr_get_sb2, -1);
    rb_define_private_method(oci8_cOCIHandle, "attr_get_sb4", attr_get_sb4, -1);
    rb_define_private_method(oci8_cOCIHandle, "attr_get_sb8", attr_get_sb8, -1);
    rb_define_private_method(oci8_cOCIHandle, "attr_get_boolean", attr_get_boolean, -1);
    rb_define_private_method(oci8_cOCIHandle, "attr_get_string", attr_get_string, -1);
    rb_define_private_method(oci8_cOCIHandle, "attr_get_binary", attr_get_binary, -1);
    rb_define_private_method(oci8_cOCIHandle, "attr_get_integer", attr_get_integer, -1);
    rb_define_private_method(oci8_cOCIHandle, "attr_get_oradate", attr_get_oradate, -1);

    /* methods to set attributes */
    rb_define_private_method(oci8_cOCIHandle, "attr_set_ub1", attr_set_ub1, 2);
    rb_define_private_method(oci8_cOCIHandle, "attr_set_ub2", attr_set_ub2, 2);
    rb_define_private_method(oci8_cOCIHandle, "attr_set_ub4", attr_set_ub4, 2);
    rb_define_private_method(oci8_cOCIHandle, "attr_set_ub8", attr_set_ub8, 2);
    rb_define_private_method(oci8_cOCIHandle, "attr_set_sb1", attr_set_sb1, 2);
    rb_define_private_method(oci8_cOCIHandle, "attr_set_sb2", attr_set_sb2, 2);
    rb_define_private_method(oci8_cOCIHandle, "attr_set_sb4", attr_set_sb4, 2);
    rb_define_private_method(oci8_cOCIHandle, "attr_set_sb8", attr_set_sb8, 2);
    rb_define_private_method(oci8_cOCIHandle, "attr_set_boolean", attr_set_boolean, 2);
    rb_define_private_method(oci8_cOCIHandle, "attr_set_string", attr_set_string, 2);
    rb_define_private_method(oci8_cOCIHandle, "attr_set_binary", attr_set_binary, 2);
    rb_define_private_method(oci8_cOCIHandle, "attr_set_integer", attr_set_integer, 2);
}
