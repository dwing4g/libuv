#ifdef ENABLE_JNI

#include <stdlib.h>
#include <string.h>
#include <jni.h>
#include "uv.h"

#define DEF_JAVA(F) Java_jane_test_net_Libuv_ ## F

#define RECV_BUF_SIZE (64 * 1024)

#define CLOSE_FROM_ACTIVE     (0)
#define CLOSE_FROM_CONNECT    (1)
#define CLOSE_FROM_RECV_START (2)
#define CLOSE_FROM_RECV       (3)
#define CLOSE_FROM_SEND       (4)

typedef struct
{
	uv_loop_t loop;
	JNIEnv* jenv;
	jobject handler; // LibuvLoopHandler
	jmethodID mid_onOpen;
	jmethodID mid_onClose;
	jmethodID mid_onRecv;
	jmethodID mid_onSend;
	jmethodID mid_onException;
	char recv_buf[RECV_BUF_SIZE]; // only one recv buffer per loop (uv_active_tcp_streams_threshold in tcp.c must be 0)
} jni_uv_loop_t; // uv_loop_t

typedef struct
{
	uv_write_t write;
	uv_buf_t buf;
	jobject buffer; // DirectByteBuffer
} jni_uv_write_req_t; // uv_write_t, uv_req_t

typedef struct
{
	uv_tcp_t tcp;
	jni_uv_write_req_t req;
	int close_from;
	int close_errcode;
} jni_uv_tcp_t; // uv_tcp_t, uv_stream_t, uv_handle_t

// public static native long libuv_loop_create(LibuvLoopHandler handler);
JNIEXPORT jlong JNICALL DEF_JAVA(libuv_1loop_1create)(JNIEnv* jenv, jclass jcls, jobject handler)
{
	jni_uv_loop_t* jloop;
	jclass cls;
	if(!handler) return 0;
	jloop = (jni_uv_loop_t*)malloc(sizeof(jni_uv_loop_t));
	if(!jloop) return 0;

	cls = (*jenv)->FindClass(jenv, "jane/net/Libuv$LibuvLoopHandler");
	if(!cls)
	{
		free(jloop);
		return 0;
	}
	cls = (jclass)(*jenv)->NewGlobalRef(jenv, cls);
	if(!cls)
	{
		free(jloop);
		return 0;
	}
	jloop->mid_onOpen      = (*jenv)->GetMethodID(jenv, cls, "onOpen",      "(JLjava/lang/String;I)V");
	jloop->mid_onClose     = (*jenv)->GetMethodID(jenv, cls, "onClose",     "(JII)V");
	jloop->mid_onRecv      = (*jenv)->GetMethodID(jenv, cls, "onRecv",      "(JI)V");
	jloop->mid_onSend      = (*jenv)->GetMethodID(jenv, cls, "onSend",      "(JLjava/nio/ByteBuffer;)V");
	jloop->mid_onException = (*jenv)->GetMethodID(jenv, cls, "onException", "(JLjava/lang/Throwable;)V");
	if(!jloop->mid_onOpen || !jloop->mid_onClose || !jloop->mid_onRecv || !jloop->mid_onSend || !jloop->mid_onException)
	{
		free(jloop);
		return 0;
	}

	jloop->jenv = jenv;
	jloop->handler = (*jenv)->NewGlobalRef(jenv, handler);
	if(!jloop->handler)
	{
		free(jloop);
		return 0;
	}
	if(uv_loop_init(&jloop->loop))
	{
		(*jenv)->DeleteGlobalRef(jenv, jloop->handler);
		free(jloop);
		return 0;
	}
	return (jlong)jloop;
}

// public static native int libuv_loop_destroy(long handle_loop);
JNIEXPORT jint JNICALL DEF_JAVA(libuv_1loop_1destroy)(JNIEnv* jenv, jclass jcls, jlong handle_loop)
{
	int r;
	jni_uv_loop_t* jloop = (jni_uv_loop_t*)handle_loop;
	if(!jloop) return EBADF;
	r = uv_loop_close(&jloop->loop);
	if(r) return r;
	(*jenv)->DeleteGlobalRef(jenv, jloop->handler);
	free(jloop);
	return 0; // uv_strerror(r)
}

// public static native ByteBuffer libuv_loop_buffer(long handle_loop);
JNIEXPORT jobject JNICALL DEF_JAVA(libuv_1loop_1buffer)(JNIEnv* jenv, jclass jcls, jlong handle_loop)
{
	jni_uv_loop_t* jloop = (jni_uv_loop_t*)handle_loop;
	return jloop ? (*jenv)->NewDirectByteBuffer(jenv, jloop->recv_buf, RECV_BUF_SIZE) : 0;
}

// public static native int libuv_loop_run(long handle_loop, int mode);
JNIEXPORT jint JNICALL DEF_JAVA(libuv_1loop_1run)(JNIEnv* jenv, jclass jcls, jlong handle_loop, jint mode)
{
	jni_uv_loop_t* jloop = (jni_uv_loop_t*)handle_loop;
	return jloop ? uv_run(&jloop->loop, (uv_run_mode)mode) : -1;
}

static int check_exception(JNIEnv* jenv, jni_uv_loop_t* jloop, jni_uv_tcp_t* jtcp)
{
	jthrowable ex = (*jenv)->ExceptionOccurred(jenv);
	if(!ex) return 0;
	(*jenv)->ExceptionClear(jenv);
	(*jenv)->CallVoidMethod(jenv, jloop->handler, jloop->mid_onException, (jlong)jtcp, ex);
	// if((*jenv)->ExceptionCheck(jenv) == JNI_TRUE)
	(*jenv)->ExceptionClear(jenv); // ignore any exception in onException
	return 1;
}

static void on_alloc(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf)
{
	buf->base = ((jni_uv_loop_t*)handle->loop)->recv_buf;
	buf->len = RECV_BUF_SIZE;
}

static void on_close(uv_handle_t* handle)
{
	jni_uv_tcp_t* jtcp = (jni_uv_tcp_t*)handle;
	jni_uv_loop_t* jloop = (jni_uv_loop_t*)jtcp->tcp.loop;
	JNIEnv* jenv = jloop->jenv;
	int from = jtcp->close_from;
	int errcode = jtcp->close_errcode;
	free(jtcp);
	(*jenv)->CallVoidMethod(jenv, jloop->handler, jloop->mid_onClose, (jlong)jtcp, from, errcode);
	// if((*jenv)->ExceptionCheck(jenv) == JNI_TRUE)
	(*jenv)->ExceptionClear(jenv); // ignore any exception in onClose
}

static void on_write(uv_write_t* req, int status)
{
	jni_uv_write_req_t* wr = (jni_uv_write_req_t*)req;
	jni_uv_tcp_t* jtcp = (jni_uv_tcp_t*)wr->write.handle;
	jni_uv_loop_t* jloop = (jni_uv_loop_t*)jtcp->tcp.loop;
	JNIEnv* jenv = jloop->jenv;
	jobject buffer;

	if(status)
	{
		(*jenv)->DeleteGlobalRef(jenv, wr->buffer);
		wr->buffer = 0;
		jtcp->close_from = CLOSE_FROM_SEND;
		jtcp->close_errcode = status; // uv_strerror(status)
		uv_close((uv_handle_t*)jtcp, on_close);
		return;
	}

	buffer = (*jenv)->NewLocalRef(jenv, wr->buffer);
	(*jenv)->DeleteGlobalRef(jenv, wr->buffer);
	wr->buffer = 0;
	(*jenv)->CallVoidMethod(jenv, jloop->handler, jloop->mid_onSend, (jlong)jtcp, buffer);
	check_exception(jenv, jloop, jtcp);
}

static void on_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf)
{
	jni_uv_tcp_t* jtcp = (jni_uv_tcp_t*)stream;
	jni_uv_loop_t* jloop;
	JNIEnv* jenv;
	if(nread < 0)
	{
		jtcp->close_from = CLOSE_FROM_RECV;
		jtcp->close_errcode = (int)nread; // UV_EOF,uv_err_name(nread)
		uv_close((uv_handle_t*)jtcp, on_close);
		return;
	}
	jloop = (jni_uv_loop_t*)jtcp->tcp.loop;
	jenv = jloop->jenv;
	(*jenv)->CallVoidMethod(jenv, jloop->handler, jloop->mid_onRecv, (jlong)jtcp, nread);
	check_exception(jenv, jloop, jtcp);
}

static void do_open(jni_uv_tcp_t* jtcp)
{
	struct sockaddr_in sa;
	int sa_size = sizeof(sa);
	int r;
	char ip[64] = {0};
	jstring jip;
	jni_uv_loop_t* jloop = (jni_uv_loop_t*)jtcp->tcp.loop;
	JNIEnv* jenv = jloop->jenv;

	uv_tcp_nodelay(&jtcp->tcp, 0);
	uv_tcp_keepalive(&jtcp->tcp, 0, 0);
	uv_tcp_simultaneous_accepts(&jtcp->tcp, 1);

	memset(&sa, 0, sa_size);
	uv_tcp_getpeername(&jtcp->tcp, (struct sockaddr*)&sa, &sa_size); // ignore return
	uv_inet_ntop(sa.sin_family, &sa.sin_addr, ip, sizeof(ip)); // ignore return
	jip = (*jenv)->NewStringUTF(jenv, ip);
	(*jenv)->CallVoidMethod(jenv, jloop->handler, jloop->mid_onOpen, (jlong)jtcp, jip, ntohs(sa.sin_port));
	if(check_exception(jenv, jloop, jtcp)) return;

	r = uv_read_start((uv_stream_t*)jtcp, on_alloc, on_read);
	if(r)
	{
		jtcp->close_from = CLOSE_FROM_RECV_START;
		jtcp->close_errcode = r;
		uv_close((uv_handle_t*)jtcp, on_close);
	}
}

static void on_accept(uv_stream_t* stream, int status)
{
	uv_tcp_t* tcp = (uv_tcp_t*)stream;
	jni_uv_tcp_t* jtcp;
	if(status) return; //TODO: uv_strerror(status)
	jtcp = (jni_uv_tcp_t*)malloc(sizeof(jni_uv_tcp_t));
	if(!jtcp) return; //TODO
	jtcp->req.buffer = 0;
	jtcp->close_from = 0;
	jtcp->close_errcode = 0;
	if(uv_tcp_init(tcp->loop, &jtcp->tcp))
	{
		free(jtcp);
		//TODO
		return;
	}
	if(uv_accept(stream, (uv_stream_t*)jtcp))
	{
		free(jtcp);
		//TODO
		return;
	}
	do_open(jtcp);
}

static void on_connect(uv_connect_t* conn, int status)
{
	jni_uv_tcp_t* jtcp = (jni_uv_tcp_t*)conn->handle;
	free(conn);
	if(status) // uv_strerror(status)
	{
		jtcp->close_from = CLOSE_FROM_CONNECT;
		jtcp->close_errcode = status;
		uv_close((uv_handle_t*)jtcp, on_close);
		return;
	}
	do_open(jtcp);
}

// public static native int libuv_tcp_bind(long handle_loop, String ip, int port, int backlog);
JNIEXPORT jint JNICALL DEF_JAVA(libuv_1tcp_1bind)(JNIEnv* jenv, jclass jcls, jlong handle_loop, jstring ip, jint port, jint backlog)
{
	int r;
	jni_uv_loop_t* jloop;
	uv_tcp_t* tcp;
	const char* ip_str;
	struct sockaddr_in sa;

	jloop = (jni_uv_loop_t*)handle_loop;
	if(!jloop) return EBADF;
	tcp = (uv_tcp_t*)malloc(sizeof(uv_tcp_t));
	if(!tcp) return ENOMEM;

	if((r = uv_tcp_init(&jloop->loop, tcp))) goto err_;
	ip_str = (ip ? (*jenv)->GetStringUTFChars(jenv, ip, 0) : 0);
	if(ip_str)
	{
		r = uv_ip4_addr(ip_str, port, &sa);
		(*jenv)->ReleaseStringUTFChars(jenv, ip, ip_str);
		if(r) goto err_;
	}
	else
		if((r = uv_ip4_addr("0.0.0.0", port, &sa))) goto err_;
	if((r = uv_tcp_bind(tcp, (struct sockaddr*)&sa, 0))) goto err_;
	if((r = uv_listen((uv_stream_t*)tcp, backlog, on_accept))) goto err_;
	return 0;
err_:
	free(tcp);
	return r; // uv_strerror(r)
}

// public static native int libuv_tcp_connect(long handle_loop, String ip, int port);
JNIEXPORT jint JNICALL DEF_JAVA(libuv_1tcp_1connect)(JNIEnv* jenv, jclass jcls, jlong handle_loop, jstring ip, jint port)
{
	int r;
	jni_uv_loop_t* jloop;
	jni_uv_tcp_t* jtcp;
	uv_connect_t* conn;
	const char* ip_str;
	struct sockaddr_in sa;

	jloop = (jni_uv_loop_t*)handle_loop;
	if(!jloop) return EBADF;
	jtcp = (jni_uv_tcp_t*)malloc(sizeof(jni_uv_tcp_t));
	if(!jtcp) return ENOMEM;
	conn = (uv_connect_t*)malloc(sizeof(uv_connect_t));
	if(!conn)
	{
		free(jtcp);
		return ENOMEM;
	}
	jtcp->req.buffer = 0;
	jtcp->close_from = 0;
	jtcp->close_errcode = 0;

	if((r = uv_tcp_init(&jloop->loop, &jtcp->tcp))) goto err_;
	ip_str = (ip ? (*jenv)->GetStringUTFChars(jenv, ip, 0) : 0);
	if(ip_str)
	{
		r = uv_ip4_addr(ip_str, port, &sa);
		(*jenv)->ReleaseStringUTFChars(jenv, ip, ip_str);
		if(r) goto err_;
	}
	else
		if((r = uv_ip4_addr("0.0.0.0", port, &sa))) goto err_;
	if((r = uv_tcp_connect(conn, &jtcp->tcp, (struct sockaddr*)&sa, on_connect))) goto err_;
	return 0;
err_:
	free(conn);
	free(jtcp);
	return r; // uv_strerror(r)
}

// public static native int libuv_tcp_send(long handle_stream, ByteBuffer buffer, int pos, int len);
JNIEXPORT jint JNICALL DEF_JAVA(libuv_1tcp_1send)(JNIEnv* jenv, jclass jcls, jlong handle_stream, jobject buffer, jint pos, jint len)
{
	int r;
	void* buf;
	jni_uv_tcp_t* jtcp = (jni_uv_tcp_t*)handle_stream;
	if(!jtcp || !buffer) return EBADF;
	buf = (*jenv)->GetDirectBufferAddress(jenv, buffer);
	if(!buf) return EBADF;
	if(jtcp->req.buffer) return ENOENT;
	jtcp->req.buffer = (*jenv)->NewGlobalRef(jenv, buffer);
	if(!jtcp->req.buffer) return EBADF;
	jtcp->req.buf = uv_buf_init((char*)buf + pos, (unsigned int)len);
	r = uv_write(&jtcp->req.write, (uv_stream_t*)jtcp, &jtcp->req.buf, 1, on_write);
	if(r)
	{
		(*jenv)->DeleteGlobalRef(jenv, jtcp->req.buffer);
		jtcp->req.buffer = 0;
	}
	return r; // uv_strerror(r)
}

// public static native int libuv_tcp_close(long handle_stream, int errcode);
JNIEXPORT jint JNICALL DEF_JAVA(libuv_1tcp_1close)(JNIEnv* jenv, jclass jcls, jlong handle_stream, jint errcode)
{
	jni_uv_tcp_t* jtcp = (jni_uv_tcp_t*)handle_stream;
	if(!jtcp) return EBADF;
	jtcp->close_from = CLOSE_FROM_ACTIVE;
	jtcp->close_errcode = errcode;
	uv_close((uv_handle_t*)jtcp, on_close);
	return 0;
}

#endif
