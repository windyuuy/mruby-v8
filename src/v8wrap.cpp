#include <v8.h>
#include <iostream>
#include <sstream>
#include <cstring>
#include <cstdlib>
#include "v8wrap.h"

extern "C" {

static v8wrap_callback _v8wrap_callback = NULL;

static std::string
to_json(v8::Handle<v8::Value> value) {
  v8::HandleScope scope;
  v8::TryCatch try_catch;
  v8::Handle<v8::Object> json = v8::Handle<v8::Object>::Cast(
    v8::Context::GetCurrent()->Global()->Get(v8::String::New("JSON")));
  v8::Handle<v8::Function> func = v8::Handle<v8::Function>::Cast(
    json->GetRealNamedProperty(v8::String::New("stringify")));
  v8::Handle<v8::Value> args[1];
  args[0] = value;
  v8::String::Utf8Value ret(
    func->Call(v8::Context::GetCurrent()->Global(), 1, args)->ToString());
  return (char*) *ret;
}

v8::Handle<v8::Value>
from_json(std::string str) {
  v8::HandleScope scope;
  v8::TryCatch try_catch;
  v8::Handle<v8::Object> json = v8::Handle<v8::Object>::Cast(
    v8::Context::GetCurrent()->Global()->Get(v8::String::New("JSON")));
  v8::Handle<v8::Function> func = v8::Handle<v8::Function>::Cast(
    json->GetRealNamedProperty(v8::String::New("parse")));
  v8::Handle<v8::Value> args[1];
  args[0] = v8::String::New(str.c_str());
  return func->Call(v8::Context::GetCurrent()->Global(), 1, args);
}

v8::Handle<v8::Value>
_mrb_v8_call(const v8::Arguments& args) {
  v8::String::Utf8Value id(args[0]);
  v8::String::Utf8Value name(args[1]);
  v8::String::Utf8Value argv(args[2]);
  v8::TryCatch try_catch;
  char* retv;
  retv = _v8wrap_callback(*id, *name, *argv);
  if (retv != NULL) {
    v8::Handle<v8::Value> ret = from_json(retv);
    free(retv);
    return ret;
  }
  return v8::Undefined();
}

class V8Context {
public:
  V8Context() {
    v8::HandleScope scope;
    global_ = v8::Persistent<v8::ObjectTemplate>::New(v8::ObjectTemplate::New());
    global_->Set(v8::String::New("_mrb_v8_call"),
      v8::FunctionTemplate::New(_mrb_v8_call));
    v8::Handle<v8::Context> context = v8::Context::New(NULL, global_);
    context_ = v8::Persistent<v8::Context>::New(context);
  };

  virtual ~V8Context() {
    context_.Dispose();
    global_.Dispose();
  };
  v8::Handle<v8::Context> context() { return context_; };
  std::string err() const { return err_; };
  void err(const std::string err) { this->err_ = err; }

private:
  v8::Persistent<v8::ObjectTemplate> global_;
  v8::Persistent<v8::Context> context_;
  v8::HandleScope handle_scope_;
  std::string err_;
};

void
v8_init(v8wrap_callback p) {
  v8::HandleScope scope;
  _v8wrap_callback = p;
}

void*
v8_create() {
  return (void*) new V8Context(); 
}

void
v8_release(void* ctx) {
  delete static_cast<V8Context *>(ctx);
}

char*
v8_error(void* ctx) {
  V8Context *context = static_cast<V8Context *>(ctx);
  return strdup(context->err().c_str());
}

static std::string
report_exception(v8::TryCatch& try_catch) {
  v8::Handle<v8::Message> message = try_catch.Message();
  v8::String::Utf8Value exception(try_catch.Exception());
  std::stringstream ss;
  if (message.IsEmpty()) {
    ss << *exception << std::endl;
  } else {
    v8::String::Utf8Value filename(message->GetScriptResourceName());
    const char* filename_string = *filename;
    int linenum = message->GetLineNumber();
    ss
      << filename_string
      << ":" << linenum
      << ": " << *exception << std::endl;
    v8::String::Utf8Value sourceline(message->GetSourceLine());
    ss << *sourceline << std::endl;
    int start = message->GetStartColumn();
    for (int n = 0; n < start; n++) {
      ss << " ";
    }
    int end = message->GetEndColumn();
    for (int n = start; n < end; n++) {
      ss << "^";
    }
    ss << std::endl;
    v8::String::Utf8Value stack_trace(try_catch.StackTrace());
    if (stack_trace.length() > 0) {
      const char* stack_trace_string = *stack_trace;
      ss << stack_trace_string << std::endl;
    }
  }
  return ss.str();
}

char*
v8_execute(void *ctx, const char* source) {
  V8Context *context = static_cast<V8Context *>(ctx);
  v8::Context::Scope context_scope(context->context());
  v8::TryCatch try_catch;

  context->err("");
  v8::Handle<v8::Script> script
    = v8::Script::Compile(v8::String::New(source));
  if (script.IsEmpty()) {
    v8::ThrowException(try_catch.Exception());
    context->err(report_exception(try_catch));
    return NULL;
  } else {
    v8::Handle<v8::Value> result = script->Run();
    if (result.IsEmpty()) {
      v8::ThrowException(try_catch.Exception());
      context->err(report_exception(try_catch));
      return NULL;
    }
    else if (result->IsFunction() || result->IsUndefined()) {
      return strdup("");
  } else {
      return strdup(to_json(result).c_str());
    }
  }
}

}

// vim:set et sw=2 ts=2 ai: