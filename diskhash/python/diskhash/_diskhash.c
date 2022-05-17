// Copyright (C) 2017  Luis Pedro Coelho <luis@luispedro.org>
//
// License: MIT (see COPYING file)

#include <Python.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "../../src/diskhash.h"

typedef struct {
    PyObject_HEAD
    HashTable* ht;
    unsigned object_size;
} htObject;


PyObject* htLookup(htObject* self, PyObject* args) {
    const char* k;
    if (!PyArg_ParseTuple(args, "s", &k)) {
        return NULL;
    }
    void* data = dht_lookup(self->ht, k);
    if (!data) {
        Py_RETURN_NONE;
    }
    return PyMemoryView_FromMemory(data, self->object_size, PyBUF_READ);
}

PyObject* htReserve(htObject* self, PyObject* args) {
    int cap;
    if (!PyArg_ParseTuple(args, "i", &cap)) {
        return NULL;
    }
    char* err;
    long r = dht_reserve(self->ht, cap, &err);
    if (r == 0) {
        if (!err) {
            return PyErr_NoMemory();
        }
        PyErr_SetString(PyExc_RuntimeError, err);
        free(err);
        return NULL;
    }
    return PyLong_FromLong(r);
}

PyObject* htInsert(htObject* self, PyObject* args) {
    const char* k;
    PyObject* v;
    if (!PyArg_ParseTuple(args, "sO", &k, &v)) {
        return NULL;
    }
    if (!PyMemoryView_Check(v)) {
        PyErr_SetString(PyExc_TypeError, "Diskhash.insert expected a memory view");
        return NULL;
    }
    Py_buffer* buf = PyMemoryView_GET_BUFFER(v);
    char* err;
    int r = dht_insert(self->ht, k, buf->buf, &err);
    if (r < 0) {
        if (!err) {
            return PyErr_NoMemory();
        }
        PyErr_SetString(PyExc_RuntimeError, err);
        free(err);
        return NULL;
    }
    return PyLong_FromLong(r);
}

PyObject* htLen(htObject* self, PyObject* args) {
    long n = dht_size(self->ht);
    return PyLong_FromLong(n);
}

static PyMethodDef htMethods[] = {
    { "lookup", (PyCFunction)htLookup, METH_VARARGS,
		    "Lookup a value.\n"
		    "\n"
		    "Returns value if found, otherwise None.\n" },

    { "reserve", (PyCFunction)htReserve, METH_VARARGS,
		    "Reserve space\n"
		    "\n"
		    "Parameters\n"
		    "----------\n"
		    "\n"
		    "c: int\n"
		    "    Desired capacity\n"
		    "\n"
		    "Returns\n"
		    "-------\n"
		    "c : int\n"
		    "   New capacity.\n" },

    { "insert", (PyCFunction)htInsert, METH_VARARGS,
		    "Insert an element into the hash.\n"
		    "\n"
		    "This function can fail (raising an exception) if there is not enough \n"
		    "capacity and hash cannot be resized.\n"
		    "\n"
		    "Parameters\n"
		    "----------\n"
		    "\n"
		    "key : str\n"
		    "    Key to insert\n"
		    "value : memoryview\n"
		    "    Value to insert\n"
		    "\n"
		    "Returns\n"
		    "-------\n"
		    "r : int\n"
		    "   1 if object was inserted, 0 if not.\n" },

    { "size", (PyCFunction)htLen, METH_VARARGS,
		    "Return number of elements." },

    {NULL}  /* Sentinel */
};



static PyObject *
htNew(PyTypeObject *type, PyObject * args, PyObject * kwargs) {
    htObject *self;

    self = (htObject *)type->tp_alloc(type, 0);
    if (self != NULL) {
        self->ht = 0;
    }

    return (PyObject *)self;
}

static int
htInit(htObject *self, PyObject *args, PyObject *kwds) {
    const char* fpath;
    const char* mode;
    int maxi;
    int object_size;
    int load;
    if (!PyArg_ParseTuple(args, "siisi", &fpath, &maxi, &object_size, &mode, &load)) {
        return -1;
    }
    int mode_flags = 0;
    if (!strcmp(mode, "r")) {
        mode_flags = O_RDONLY;
    } else if (!strcmp(mode, "w")
                || !strcmp(mode, "rw")
                || !strcmp(mode, "wr")
                || !strcmp(mode, "+")) {
        mode_flags = O_RDWR|O_CREAT;
    } else if (!strcmp(mode, "a")) {
        mode_flags = O_RDWR;
    } else if (!strcmp(mode, "x")) {
        mode_flags = O_RDWR|O_CREAT|O_EXCL;
    }

    HashTableOpts opts;
    opts.key_maxlen = maxi;
    opts.object_datalen = object_size;

    char* err;
    self->ht = dht_open(fpath, opts, mode_flags, &err);
    self->object_size = object_size;

    if (!self->ht) {
        if (!err) {
            PyErr_SetNone(PyExc_MemoryError);
        } else {
            PyErr_SetString(PyExc_RuntimeError, err);
            free(err);
        }
        return -1;
    }
    if (load) {
        int e = dht_load_to_memory(self->ht, &err);
        if (e == 2) {
            PyErr_SetString(PyExc_RuntimeError, err);
            return -1;
        }
    }
    return 0;
}


static void
htDealloc(PyObject* obj) {
    htObject* ht = (htObject*)obj;
    if (ht->ht) dht_free(ht->ht);
}



static PyTypeObject htWrapperType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "diskhash.Diskhash",       /* tp_name */
    sizeof(htObject),          /* tp_basicsize */
    0,                         /* tp_itemsize */
    htDealloc,                 /* tp_dealloc */
    0,                         /* tp_print */
    0,                         /* tp_getattr */
    0,                         /* tp_setattr */
    0,                         /* tp_reserved */
    0,                         /* tp_repr */
    0,                         /* tp_as_number */
    0,                         /* tp_as_sequence */
    0,                         /* tp_as_mapping */
    0,                         /* tp_hash  */
    0,                         /* tp_call */
    0,                         /* tp_str */
    0,                         /* tp_getattro */
    0,                         /* tp_setattro */
    0,                         /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT |
        Py_TPFLAGS_BASETYPE,   /* tp_flags */

    "Disk based hash table.\n"
    "\n"
    "See https://github.com/luispedro/diskhash\n", /* tp_doc */

    0,                         /* tp_traverse */
    0,                         /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,                         /* tp_iternext */
    htMethods,                 /* tp_methods */
    0,                         /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)htInit,          /* tp_init */
    0,                         /* tp_alloc */
    htNew,                     /* tp_new */
};

static PyModuleDef pydiskhash = {
    PyModuleDef_HEAD_INIT,
    "diskhash",
    "",
    -1,
    NULL, NULL, NULL, NULL, NULL
};

PyMODINIT_FUNC
PyInit__diskhash(void)
{
    PyObject* m;

    htWrapperType.tp_new = PyType_GenericNew;
    if (PyType_Ready(&htWrapperType) < 0)
        return NULL;

    m = PyModule_Create(&pydiskhash);
    if (m == NULL)
        return NULL;

    Py_INCREF(&htWrapperType);
    PyModule_AddObject(m, "Diskhash", (PyObject *)&htWrapperType);
    return m;
}



