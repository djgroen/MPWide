%module MPWide

%ignore MPW_Init(string*, int*, int*, int);
%ignore MPW_Init(string, int);
%ignore MPW_Init(string);
%ignore MPW_Init();

%ignore MPW_SendRecv(char* sendbuf, long long int sendsize, char* recvbuf, long long int recvsize, int channel); 

// This tells SWIG to treat char ** as a special case
%typemap(in) char ** {
  /* Check if is a list */
  if (PyList_Check($input)) {
    int size = PyList_Size($input);
    int i = 0;
    $1 = (char **) malloc((size)*sizeof(char *));
    for (i = 0; i < size; i++) {
      PyObject *o = PyList_GetItem($input,i);
      if (PyString_Check(o))
    $1[i] = PyString_AsString(PyList_GetItem($input,i));
      else {
    PyErr_SetString(PyExc_TypeError,"list must contain strings");
    free($1);
    return NULL;
      }
    }
  } else {
    PyErr_SetString(PyExc_TypeError,"not a list");
    return NULL;
  }
}

%typemap(freearg) char ** {
  free((char **) $1);
}


// This tells SWIG to treat string * as a special case
%typemap(in) string * {
  /* Check if is a list */
  if (PyList_Check($input)) {
    int size = PyList_Size($input);
    int i = 0;
    $1 = new string[size];
    for (i = 0; i < size; i++) {
      PyObject *o = PyList_GetItem($input,i);
      if (PyString_Check(o)) {
        string tmp = PyString_AsString(o);
        $1[i] = tmp;
      } else {
        PyErr_SetString(PyExc_TypeError,"list must contain strings");
        delete [] $1;
        return NULL;
      }
    }
  } else {
    PyErr_SetString(PyExc_TypeError,"not a list");
    return NULL;
  }
}

%typemap(freearg) string * {
  delete [] $1;
}


// This tells SWIG to treat int * as a special case
%typemap(in) int * {
  /* Check if is a list */
  if (PyList_Check($input)) {
    int size = PyList_Size($input);
    int i = 0;
    $1 = (int *) malloc((size)*sizeof(int));
    for (i = 0; i < size; i++) {
      PyObject *o = PyList_GetItem($input,i);
      if (PyInt_Check(o)) {
        int tmp = (int) PyInt_AsLong(PyList_GetItem($input,i));
        $1[i] = tmp;
      } else {
        PyErr_SetString(PyExc_TypeError,"list must contain longs/ints");
        free($1);
        return NULL;
      }
    }
  } else {
    PyErr_SetString(PyExc_TypeError,"not a list");
    return NULL;
  }
}


// This cleans up the int * array we mallocd before the function call
%typemap(freearg) int * {
  free((int *) $1);
}

//void MPW_Init(string* url, int* server_side_ports, int num_channels);
%include ../MPWide.h
%{
#include "../MPWide.h"
  %}
