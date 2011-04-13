#ifndef MODULE_HPP
#define MODULE_HPP

#include <Python.h>

#include <iostream>

#include <google/protobuf/io/zero_copy_stream_impl.h>

namespace mesos { namespace python {

/**
 * The Python module object for mesos_pb2 (which contains the protobuf
 * classes generated for Python).
 */
extern PyObject* mesos_pb2;


/**
 * RAII utility class for acquiring the Python global interpreter lock.
 */
class InterpreterLock {
  PyGILState_STATE state;

public:
  InterpreterLock() {
    state = PyGILState_Ensure();
  }

  ~InterpreterLock() {
    PyGILState_Release(state);
  }
};


/**
 * Convert a Python protocol buffer object into a C++ one by serializing
 * it to a string and deserializing the result back in C++. Returns true
 * on success, or prints an error and returns false on failure.
 */
template <typename T>
bool readPythonProtobuf(PyObject* obj, T* t)
{
  if (obj == Py_None) {
    std::cerr << "None object given where protobuf expected" << std::endl;
    return false;
  }
  PyObject* res = PyObject_CallMethod(obj,
                                      (char*) "SerializeToString",
                                      (char*) NULL);
  if (res == NULL) {
    std::cerr << "Failed to call Python object's SerializeToString "
         << "(perhaps it is not a protobuf?)" << std::endl;
    PyErr_Print();
    return false;
  }
  char* chars;
  Py_ssize_t len;
  if (PyString_AsStringAndSize(res, &chars, &len) < 0) {
    std::cerr << "SerializeToString did not return a string" << std::endl;
    PyErr_Print();
    Py_DECREF(res);
    return false;
  }
  google::protobuf::io::ArrayInputStream stream(chars, len);
  bool success = t->ParseFromZeroCopyStream(&stream);
  if (!success) {
    std::cerr << "Could not deserialize protobuf as expected type" << std::endl;
  }
  Py_DECREF(res);
  return success;
}


/**
 * Convert a C++ protocol buffer object into a Python one by serializing
 * it to a string and deserializing the result back in Python. Returns the
 * resulting PyObject* on success or raises a Python exception and returns
 * NULL on failure.
 */
template <typename T>
PyObject* createPythonProtobuf(const T& t, const char* typeName)
{
  PyObject* dict = PyModule_GetDict(mesos_pb2);
  if (dict == NULL) {
    PyErr_Format(PyExc_Exception, "PyModule_GetDict failed");
    return NULL;
  }

  PyObject* type = PyDict_GetItemString(dict, typeName);
  if (type == NULL) {
    PyErr_Format(PyExc_Exception, "Could not resolve mesos_pb2.%s", typeName);
    return NULL;
  }
  if (!PyType_Check(type)) {
    PyErr_Format(PyExc_Exception, "mesos_pb2.%s is not a type", typeName);
    return NULL;
  }

  std::string str;
  if (!t.SerializeToString(&str)) {
    PyErr_Format(PyExc_Exception, "C++ %s SerializeToString failed", typeName);
    return NULL;
  }

  // Propagates any exception that might happen in FromString
  return PyObject_CallMethod(type,
                             (char*) "FromString",
                             (char*) "s#",
                             str.data(),
                             str.size());
}

}} /* namespace mesos { namespace python { */

#endif /* MODULE_HPP */