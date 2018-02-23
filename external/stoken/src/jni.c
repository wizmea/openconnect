/*
 * jni.c - stoken Java Native Interface
 *
 * Copyright 2014 Kevin Cernekee <cernekee@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "config.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <jni.h>
#include "stoken.h"

/* these need to match LibStoken.java */
#define SUCCESS			( 0)
#define INVALID_FORMAT		(-1)
#define IO_ERROR		(-2)
#define FILE_NOT_FOUND		(-3)

struct libctx {
	JNIEnv *jenv;
	jobject jobj;
	struct stoken_ctx *instance;
};

static void throw_excep(JNIEnv *jenv, const char *exc, int line)
{
	jclass excep;
	char msg[64];

	snprintf(msg, 64, "%s:%d", __FILE__, line);

	(*jenv)->ExceptionClear(jenv);
	excep = (*jenv)->FindClass(jenv, exc);
	if (excep)
		(*jenv)->ThrowNew(jenv, excep, msg);
}

#define OOM(jenv)	do { throw_excep(jenv, "java/lang/OutOfMemoryError", __LINE__); } while (0)

static int translate_errno(JNIEnv *jenv, int err)
{
	switch (err) {
	case 0:
		return SUCCESS;
	case -EINVAL:
		return INVALID_FORMAT;
	case -ENOENT:
		return FILE_NOT_FOUND;
	case -ENOMEM:
		throw_excep(jenv, "java/lang/OutOfMemoryError", __LINE__);
		/* falls through */
	case -EIO:
	default:
		return IO_ERROR;
	}
}

static struct libctx *getctx(JNIEnv *jenv, jobject jobj)
{
	jclass jcls = (*jenv)->GetObjectClass(jenv, jobj);
	jfieldID jfld = (*jenv)->GetFieldID(jenv, jcls, "libctx", "J");
	if (!jfld)
		return NULL;
	return (void *)(unsigned long)(*jenv)->GetLongField(jenv, jobj, jfld);
}

static int set_int(struct libctx *ctx, jobject jobj, const char *name, int value)
{
	jclass jcls = (*ctx->jenv)->GetObjectClass(ctx->jenv, jobj);
	jfieldID jfld = (*ctx->jenv)->GetFieldID(ctx->jenv, jcls, name, "I");

	if (!jfld)
		return -1;
	(*ctx->jenv)->SetIntField(ctx->jenv, jobj, jfld, value);
	return 0;
}

static int set_long(struct libctx *ctx, jobject jobj, const char *name, uint64_t value)
{
	jclass jcls = (*ctx->jenv)->GetObjectClass(ctx->jenv, jobj);
	jfieldID jfld = (*ctx->jenv)->GetFieldID(ctx->jenv, jcls, name, "J");

	if (!jfld)
		return -1;
	(*ctx->jenv)->SetLongField(ctx->jenv, jobj, jfld, (jlong)value);
	return 0;
}

static int set_bool(struct libctx *ctx, jobject jobj, const char *name, int value)
{
	jclass jcls = (*ctx->jenv)->GetObjectClass(ctx->jenv, jobj);
	jfieldID jfld = (*ctx->jenv)->GetFieldID(ctx->jenv, jcls, name, "Z");

	if (!jfld)
		return -1;
	(*ctx->jenv)->SetBooleanField(ctx->jenv, jobj, jfld, value);
	return 0;
}

static jstring dup_to_jstring(JNIEnv *jenv, const char *in)
{
	/*
	 * Many implementations of NewStringUTF() will return NULL on
	 * NULL input, but that isn't guaranteed:
	 * http://gcc.gnu.org/bugzilla/show_bug.cgi?id=35979
	 */
	return in ? (*jenv)->NewStringUTF(jenv, in) : NULL;
}

static int set_string(struct libctx *ctx, jobject jobj, const char *name, const char *value)
{
	jclass jcls = (*ctx->jenv)->GetObjectClass(ctx->jenv, jobj);
	jfieldID jfld = (*ctx->jenv)->GetFieldID(ctx->jenv, jcls, name, "Ljava/lang/String;");
	jstring jarg;

	if (!jfld)
		return -1;

	jarg = dup_to_jstring(ctx->jenv, value);
	if (value && !jarg)
		return -1;
	(*ctx->jenv)->SetObjectField(ctx->jenv, jobj, jfld, jarg);
	return 0;
}

JNIEXPORT jlong JNICALL Java_org_stoken_LibStoken_init(
	JNIEnv *jenv, jobject jobj)
{
	struct libctx *ctx = calloc(1, sizeof(*ctx));

	if (!ctx)
		goto bad;

	ctx->jenv = jenv;
	ctx->jobj = (*jenv)->NewGlobalRef(jenv, jobj);
	if (!ctx->jobj)
		goto bad_free_ctx;

	ctx->instance = stoken_new();
	if (!ctx->instance)
		goto bad_delete_ref;

	return (jlong)(unsigned long)ctx;

bad_delete_ref:
	(*jenv)->DeleteGlobalRef(jenv, ctx->jobj);
bad_free_ctx:
	free(ctx);
bad:
	OOM(jenv);
	return 0;
}

JNIEXPORT void JNICALL Java_org_stoken_LibStoken_free(
	JNIEnv *jenv, jobject jobj)
{
	struct libctx *ctx = getctx(jenv, jobj);

	if (!ctx)
		return;
	stoken_destroy(ctx->instance);
	(*jenv)->DeleteGlobalRef(jenv, ctx->jobj);
	free(ctx);
}

JNIEXPORT jint JNICALL Java_org_stoken_LibStoken_importRCFile(
	JNIEnv *jenv, jobject jobj, jstring jarg0)
{
	struct libctx *ctx = getctx(jenv, jobj);
	const char *arg0;
	int ret;

	if (!jarg0)
		return translate_errno(jenv, -EINVAL);

	arg0 = (*jenv)->GetStringUTFChars(jenv, jarg0, NULL);
	if (!arg0)
		ret = -ENOMEM;
	else
		ret = stoken_import_rcfile(ctx->instance, arg0);

	(*jenv)->ReleaseStringUTFChars(jenv, jarg0, arg0);
	return translate_errno(jenv, ret);
}

JNIEXPORT jint JNICALL Java_org_stoken_LibStoken_importString(
	JNIEnv *jenv, jobject jobj, jstring jarg0)
{
	struct libctx *ctx = getctx(jenv, jobj);
	const char *arg0;
	int ret;

	if (!jarg0)
		return translate_errno(jenv, -EINVAL);

	arg0 = (*jenv)->GetStringUTFChars(jenv, jarg0, NULL);
	if (!arg0)
		ret = -ENOMEM;
	else
		ret = stoken_import_string(ctx->instance, arg0);

	(*jenv)->ReleaseStringUTFChars(jenv, jarg0, arg0);
	return translate_errno(jenv, ret);
}

JNIEXPORT jobject JNICALL Java_org_stoken_LibStoken_getInfo(
	JNIEnv *jenv, jobject jobj)
{
	struct libctx *ctx = getctx(jenv, jobj);
	struct stoken_info *info;
	jmethodID mid;
	jclass jcls;

	jcls = (*ctx->jenv)->FindClass(ctx->jenv,
				       "org/stoken/LibStoken$StokenInfo");
	if (jcls == NULL)
		return NULL;

	mid = (*ctx->jenv)->GetMethodID(ctx->jenv, jcls, "<init>", "()V");
	if (!mid)
		return NULL;
	jobj = (*ctx->jenv)->NewObject(ctx->jenv, jcls, mid);
	if (!jobj)
		return NULL;

	info = stoken_get_info(ctx->instance);
	if (!info)
		return NULL;

	if (set_string(ctx, jobj, "serial", info->serial) ||
	    set_long(ctx, jobj, "unixExpDate", info->exp_date) ||
	    set_int(ctx, jobj, "interval", info->interval) ||
	    set_int(ctx, jobj, "tokenVersion", info->token_version) ||
	    set_bool(ctx, jobj, "usesPin", info->uses_pin))
		jobj = NULL;

	free(info);
	return jobj;
}

JNIEXPORT jint JNICALL Java_org_stoken_LibStoken_getMinPIN(
	JNIEnv *jenv, jobject jobj)
{
	struct libctx *ctx = getctx(jenv, jobj);
	int min_pin, max_pin;

	stoken_pin_range(ctx->instance, &min_pin, &max_pin);
	return min_pin;
}

JNIEXPORT jint JNICALL Java_org_stoken_LibStoken_getMaxPIN(
	JNIEnv *jenv, jobject jobj)
{
	struct libctx *ctx = getctx(jenv, jobj);
	int min_pin, max_pin;

	stoken_pin_range(ctx->instance, &min_pin, &max_pin);
	return max_pin;
}

JNIEXPORT jboolean JNICALL Java_org_stoken_LibStoken_isPINRequired(
	JNIEnv *jenv, jobject jobj)
{
	struct libctx *ctx = getctx(jenv, jobj);
	return !!stoken_pin_required(ctx->instance);
}

JNIEXPORT jboolean JNICALL Java_org_stoken_LibStoken_isPassRequired(
	JNIEnv *jenv, jobject jobj)
{
	struct libctx *ctx = getctx(jenv, jobj);
	return !!stoken_pass_required(ctx->instance);
}

JNIEXPORT jboolean JNICALL Java_org_stoken_LibStoken_isDevIDRequired(
	JNIEnv *jenv, jobject jobj)
{
	struct libctx *ctx = getctx(jenv, jobj);
	return !!stoken_devid_required(ctx->instance);
}

JNIEXPORT jboolean JNICALL Java_org_stoken_LibStoken_checkPIN(
	JNIEnv *jenv, jobject jobj, jstring jarg0)
{
	struct libctx *ctx = getctx(jenv, jobj);
	const char *arg0;
	int ret;

	if (!jarg0)
		return translate_errno(jenv, -EINVAL);

	arg0 = (*jenv)->GetStringUTFChars(jenv, jarg0, NULL);
	if (!arg0)
		ret = -ENOMEM;
	else
		ret = stoken_check_pin(ctx->instance, arg0);

	(*jenv)->ReleaseStringUTFChars(jenv, jarg0, arg0);
	return !translate_errno(jenv, ret);
}

JNIEXPORT jboolean JNICALL Java_org_stoken_LibStoken_checkDevID(
	JNIEnv *jenv, jobject jobj, jstring jarg0)
{
	struct libctx *ctx = getctx(jenv, jobj);
	const char *arg0;
	int ret;

	if (!jarg0)
		return translate_errno(jenv, -EINVAL);

	arg0 = (*jenv)->GetStringUTFChars(jenv, jarg0, NULL);
	if (!arg0)
		ret = -ENOMEM;
	else
		ret = stoken_check_devid(ctx->instance, arg0);

	(*jenv)->ReleaseStringUTFChars(jenv, jarg0, arg0);
	return !translate_errno(jenv, ret);
}

JNIEXPORT jobject JNICALL Java_org_stoken_LibStoken_getGUIDList(
	JNIEnv *jenv, jobject jobj)
{
	struct libctx *ctx = getctx(jenv, jobj);
	jmethodID mid;
	jclass jcls;
	const struct stoken_guid *guidlist = stoken_get_guid_list();
	int i, len;
	jobjectArray jarr;

	for (len = 0; guidlist[len].tag != NULL; len++)
		;

	jcls = (*ctx->jenv)->FindClass(ctx->jenv,
				       "org/stoken/LibStoken$StokenGUID");
	if (jcls == NULL)
		return NULL;

	mid = (*ctx->jenv)->GetMethodID(ctx->jenv, jcls, "<init>", "()V");
	if (!mid)
		return NULL;
	jarr = (*ctx->jenv)->NewObjectArray(ctx->jenv, len, jcls, NULL);
	if (!jarr)
		return NULL;

	for (i = 0; i < len; i++) {
		const struct stoken_guid *g = &guidlist[i];

		jobj = (*ctx->jenv)->NewObject(ctx->jenv, jcls, mid);
		if (!jobj)
			return NULL;

		if (set_string(ctx, jobj, "tag", g->tag) ||
		    set_string(ctx, jobj, "longName", g->long_name) ||
		    set_string(ctx, jobj, "GUID", g->guid))
			return NULL;

		(*ctx->jenv)->SetObjectArrayElement(ctx->jenv, jarr, i, jobj);
	}

	return jarr;
}

JNIEXPORT jint JNICALL Java_org_stoken_LibStoken_decryptSeed(
	JNIEnv *jenv, jobject jobj, jstring jarg0, jstring jarg1)
{
	struct libctx *ctx = getctx(jenv, jobj);
	const char *arg0 = NULL, *arg1 = NULL;
	int ret = -ENOMEM;

	if (jarg0) {
		arg0 = (*jenv)->GetStringUTFChars(jenv, jarg0, NULL);
		if (!arg0)
			goto out;
	}
	if (jarg1) {
		arg1 = (*jenv)->GetStringUTFChars(jenv, jarg1, NULL);
		if (!arg1)
			goto out;
	}

	ret = stoken_decrypt_seed(ctx->instance, arg0, arg1);

out:
	if (arg1)
		(*jenv)->ReleaseStringUTFChars(jenv, jarg1, arg1);
	if (arg0)
		(*jenv)->ReleaseStringUTFChars(jenv, jarg0, arg0);
	return translate_errno(jenv, ret);
}

JNIEXPORT jstring JNICALL Java_org_stoken_LibStoken_encryptSeed(
	JNIEnv *jenv, jobject jobj, jstring jarg0, jstring jarg1)
{
	struct libctx *ctx = getctx(jenv, jobj);
	const char *arg0 = NULL, *arg1 = NULL;
	char *ret;
	jstring jret = NULL;

	if (jarg0) {
		arg0 = (*jenv)->GetStringUTFChars(jenv, jarg0, NULL);
		if (!arg0)
			goto out;
	}
	if (jarg1) {
		arg1 = (*jenv)->GetStringUTFChars(jenv, jarg1, NULL);
		if (!arg1)
			goto out;
	}

	ret = stoken_encrypt_seed(ctx->instance, arg0, arg1);
	jret = ret ? (*jenv)->NewStringUTF(jenv, ret) : NULL;
	free(ret);

out:
	if (arg1)
		(*jenv)->ReleaseStringUTFChars(jenv, jarg1, arg1);
	if (arg0)
		(*jenv)->ReleaseStringUTFChars(jenv, jarg0, arg0);
	return jret;
}

JNIEXPORT jstring JNICALL Java_org_stoken_LibStoken_computeTokencode(
	JNIEnv *jenv, jobject jobj, jlong jwhen, jstring jpin)
{
	struct libctx *ctx = getctx(jenv, jobj);
	const char *pin = NULL;
	time_t when = jwhen ? jwhen : time(NULL);
	char tokencode[STOKEN_MAX_TOKENCODE + 1];
	jstring ret = NULL;

	if (jpin) {
		pin = (*jenv)->GetStringUTFChars(jenv, jpin, NULL);
		if (!pin) {
			OOM(jenv);
			return NULL;
		}
	}

	if (stoken_compute_tokencode(ctx->instance, when, pin, tokencode) == 0)
		ret = (*jenv)->NewStringUTF(jenv, tokencode);

	if (jpin)
		(*jenv)->ReleaseStringUTFChars(jenv, jpin, pin);
	return ret;
}

JNIEXPORT jstring JNICALL Java_org_stoken_LibStoken_formatTokencode(
	JNIEnv *jenv, jobject jobj, jstring jarg0)
{
	const char *arg0;
	char *ret;
	jstring jret = NULL;

	if (!jarg0)
		return NULL;

	arg0 = (*jenv)->GetStringUTFChars(jenv, jarg0, NULL);
	if (!arg0)
		return NULL;

	ret = stoken_format_tokencode(arg0);
	jret = (*jenv)->NewStringUTF(jenv, ret);

	free(ret);
	(*jenv)->ReleaseStringUTFChars(jenv, jarg0, arg0);

	return jret;
}
