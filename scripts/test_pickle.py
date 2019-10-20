#
#  This file is part of Permafrost Engine. 
#  Copyright (C) 2019 Eduard Permyakov 
#
#  Permafrost Engine is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  Permafrost Engine is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.
# 
#  Linking this software statically or dynamically with other modules is making 
#  a combined work based on this software. Thus, the terms and conditions of 
#  the GNU General Public License cover the whole combination. 
#  
#  As a special exception, the copyright holders of Permafrost Engine give 
#  you permission to link Permafrost Engine with independent modules to produce 
#  an executable, regardless of the license terms of these independent 
#  modules, and to copy and distribute the resulting executable under 
#  terms of your choice, provided that you also meet, for each linked 
#  independent module, the terms and conditions of the license of that 
#  module. An independent module is a module which is not derived from 
#  or based on Permafrost Engine. If you modify Permafrost Engine, you may 
#  extend this exception to your version of Permafrost Engine, but you are not 
#  obliged to do so. If you do not wish to do so, delete this exception 
#  statement from your version.
#

import pf
import traceback
import sys
import imp
import exceptions


def test_pickle_int():

    i1 = -259
    s = pf.pickle_object(i1)
    i2 = pf.unpickle_object(s)
    assert i1 == i2

    i1 = 0x556789
    s = pf.pickle_object(i1)
    i2 = pf.unpickle_object(s)
    assert i1 == i2

    print "Int picking OK!"

def test_pickle_long():

    l1 = long(1 << 32)
    s = pf.pickle_object(l1)
    l2 = pf.unpickle_object(s)
    assert l1 == l2

    print "Long picking OK!"

def test_pickle_string():
    
    s1 = "Hello World!"
    s = pf.pickle_object(s1)
    s2 = pf.unpickle_object(s)
    assert s1 == s2 

    s1 = ""
    s = pf.pickle_object(s1)
    s2 = pf.unpickle_object(s)
    assert s1 == s2 

    s1 = "\"This is a weird string\"\n"
    s = pf.pickle_object(s1)
    s2 = pf.unpickle_object(s)
    assert s1 == s2 

    print "String pickling OK!"

def test_pickle_unicode():

    #pf.trace_pickling = True
    u1 = u'abcdefg'
    s = pf.pickle_object(u1)
    print s
    u2 = pf.unpickle_object(s)
    assert u1 == u2

    u1 = u'\u043F\u0440\u0438\u0432\u0435\u0442'
    s = pf.pickle_object(u1)
    u2 = pf.unpickle_object(s)
    assert u1 == u2

    u1 = u'\n\n'
    s = pf.pickle_object(u1)
    u2 = pf.unpickle_object(s)
    assert u1 == u2

    print "Unicode pickling OK!"

def test_pickle_tuple():

    t1 = ()
    s = pf.pickle_object(t1)
    t2 = pf.unpickle_object(s)
    assert t1 == t2

    t1 = (0, 1, 2)
    s = pf.pickle_object(t1)
    t2 = pf.unpickle_object(s)
    assert t1 == t2 

    t1 = (0, 1, ("Hello", "World", "!", (2, 3, ())))
    s = pf.pickle_object(t1)
    t2 = pf.unpickle_object(s)
    assert t1 == t2 

    #Self referencing
    l = []
    t1 = (1, 2, l)
    l.append(t1)
    s = pf.pickle_object(t1)
    t2 = pf.unpickle_object(s)
    assert t1.__repr__() == t2.__repr__() # Can't compare direcly due to recursive def
    assert id(t2) == id(t2[2][0])

    a = 1
    t1 = (a, a, a)
    s = pf.pickle_object(t1)
    t2 = pf.unpickle_object(s)
    assert t1 == t2
    assert id(t2[0]) == id(t2[1])
    assert id(t2[1]) == id(t2[2])

    print "Tuple pickling OK!"

def test_pickle_list():

    l1 = []
    s = pf.pickle_object(l1)
    l2 = pf.unpickle_object(s)
    assert l1 == l2

    l1 = [1, 2, 3, "Hello", "World", (1, 2), [3], [[[9]]], "!", (1, [120])]
    s = pf.pickle_object(l1)
    l2 = pf.unpickle_object(s)
    assert l1 == l2

    #self referencing
    l1 = []
    l1.append(l1)
    s = pf.pickle_object(l1)
    l2 = pf.unpickle_object(s)
    assert l1.__repr__() == l2.__repr__() # Can't compare direcly due to recursive def
    assert id(l2) == id(l2[0])

    print "List pickling OK!"

def test_pickle_dict():

    d1 = {}
    s = pf.pickle_object(d1)
    d2 = pf.unpickle_object(s)
    assert d1 == d2

    a = "same",
    d1 = {
        "Hello" : "World",
        "Apple" : "Orange",
        a       : a,
        99      : 12356,
        100     : (1, 2, 3),
        101     : {0 : {}, 1 : {}}
    }
    s = pf.pickle_object(d1)
    d2 = pf.unpickle_object(s)
    assert d1 == d2

    #self referencing
    d1 = {}
    d1["key"] = d1
    s = pf.pickle_object(d1)
    d2 = pf.unpickle_object(s)
    assert d1.__repr__() == d2.__repr__()
    assert id(d2) == id(d2["key"])

    print "Dict pickling OK!"

def test_pickle_set():

    s1 = set({1,2,3,4,5,"Hello World"})
    s = pf.pickle_object(s1)
    s2 = pf.unpickle_object(s)
    assert s1 == s2

    print "Set pickling OK!"

def test_pickle_frozenset():

    f1 = frozenset({1,2,3,4,5,"Hello World"})
    s = pf.pickle_object(f1)
    f2 = pf.unpickle_object(s)
    assert f1 == f2

    print "Frozenset pickling OK!"

def test_pickle_cfunction():

    f1 = locals
    s = pf.pickle_object(f1)
    f2 = pf.unpickle_object(s)
    assert f1 == f2

    f1 = globals
    s = pf.pickle_object(f1)
    f2 = pf.unpickle_object(s)
    assert f1 == f2

    f1 = pf.pickle_object
    s = pf.pickle_object(f1)
    f2 = pf.unpickle_object(s)
    assert f1 == f2

    print "Built-in function pickling OK!"

def test_pickle_code():

    def testfunc(a, b):
        return a + b  

    c1 = testfunc.func_code
    s = pf.pickle_object(c1)
    c2 = pf.unpickle_object(s);
    assert c1 == c2

    print "Code pickling OK!"

def test_pickle_function():

    def testfunc(a, b):
        return a + b  

    f1 = testfunc
    s = pf.pickle_object(f1)
    f2 = pf.unpickle_object(s)
    assert f1(1, 3) == f2(1, 3)

    #test closure
    def outer():
        a = 5
        def inner(num):
            return a + num
        return inner

    f1 = outer()
    s = pf.pickle_object(f1)
    f2 = pf.unpickle_object(s)
    assert f1(15) == f2(15)

    #test function decorator
    def test_decorator(func):
        def triple(num):
            return 3*func(num)
        return triple

    @test_decorator
    def double(num):
        return 2*num 

    f1 = double
    s = pf.pickle_object(f1)
    f2 = pf.unpickle_object(s)
    assert f1(9) == f2(9)

    #test pickling default args
    def testfunc2(num=5, word='test'):
        return num*word

    f1 = testfunc2
    s = pf.pickle_object(f1)
    f2 = pf.unpickle_object(s)
    assert f1() == f2()
    assert f1(num=3) == f2(num=3)

    print "Function pickling OK!"

def test_pickle_class():

    class TestClass:
        clsattr = 'Hello'
        def __init__(self):
            self.a = 1
            self.b = 2
            self.c = ("Hello World", 1, 2)

    c1 = TestClass
    s = pf.pickle_object(c1)
    c2 = pf.unpickle_object(s)
    assert c1.clsattr == c2.clsattr


    print "Class pickling OK!"

def test_pickle_instance():

    class TestClass:
        clsattr = 'Hello'
        def __init__(self, a, b):
            self.a = a
            self.b = b

    i1 = TestClass(25, 'Test123')
    s = pf.pickle_object(i1)
    i2 = pf.unpickle_object(s)
    assert type(i1) == type(i2)
    assert i1.__class__.__name__ == i2.__class__.__name__
    assert i1.clsattr == i2.clsattr
    assert i1.a == i2.a
    assert i1.b == i2.b

    print "Instance pickling OK!"

def test_pickle_type():

    # Built-in types
    t1 = object
    s = pf.pickle_object(t1)
    t2 = pf.unpickle_object(s)
    assert t1 == t2

    t1 = dict
    s = pf.pickle_object(t1)
    t2 = pf.unpickle_object(s)
    assert t1 == t2

    #User-defined types (i.e. new-style classes)
    class TestClass(object):
        clsattr = 'Hello'
        def __init__(self):
            self.a = 1
            self.b = 2
            self.c = 3

    t1 = TestClass
    s = pf.pickle_object(t1)
    t2 = pf.unpickle_object(s)

    assert t1.clsattr == t2.clsattr
    i1, i2 = t1(), t2()
    assert i1.a == i2.a
    assert i1.b == i2.b
    assert i1.c == i2.c

    #Sublcasses of builtins
    class UserDict(dict):
        def __init__(self, *args):
            super(UserDict, self).__init__(*args)
            self.customattr = 123

    t1 = UserDict
    s = pf.pickle_object(t1)
    t2 = pf.unpickle_object(s)

    i1 = t1({'a':1})
    i2 = t2({'a':1})
    assert i1.customattr == i2.customattr
    assert i1['a'] == i2['a']

    #Type with '__slots__'
    class SlotsType(object):
        __slots__ = ['a', 'b']
        def __init__(self):
            self.a = 1
            self.b = 2

    t1 = SlotsType
    s = pf.pickle_object(t1)
    t2 = pf.unpickle_object(s)
    assert t1.__slots__ == t2.__slots__

    i1, i2 = t1(), t2()
    assert i1.a == i2.a
    assert i1.b == i2.b
    try:
        setattr(i2, 'c', 3)
    except AttributeError:
        pass
    else:
        raise Exception("Able to set non-slot attribute: __slots__ not properly saved")

    print "Type pickling OK!"

def test_pickle_bool():

    b1 = True
    s = pf.pickle_object(b1)
    b2 = pf.unpickle_object(s)
    assert b1 == b2

    b1 = False
    s = pf.pickle_object(b1)
    b2 = pf.unpickle_object(s)
    assert b1 == b2

    print "Bool pickling OK!"

def test_pickle_bytearray():

    b1 = bytearray('\x00\x01\x02\0\x03', 'UTF-8')
    s = pf.pickle_object(b1)
    b2 = pf.unpickle_object(s)
    assert b1 == b2

    print "Bytearray pickling OK!"

def test_pickle_baseobject():

    o1 = object()
    s = pf.pickle_object(o1)
    o2 = pf.unpickle_object(s)
    assert type(o1) == type(o2)

    print "Base object pickling OK!"

def test_pickle_notimplemented():

    n1 = NotImplemented
    s = pf.pickle_object(n1)
    n2 = pf.unpickle_object(s)

    print "NotImplemented pickling OK!"

def test_pickle_ellipsis():

    e1 = Ellipsis
    s = pf.pickle_object(e1)
    e2 = pf.unpickle_object(s)

    print "Ellipsis pickling OK!"

def test_pickle_syslonginfo():

    l1 = sys.long_info
    s = pf.pickle_object(l1)
    l2 = pf.unpickle_object(s)
    assert l1 == l2

    print "sys.long_info pickling OK!"

def test_pickle_sysfloatinfo():

    l1 = sys.float_info
    s = pf.pickle_object(l1)
    l2 = pf.unpickle_object(s)
    assert l1 == l2

    print "sys.float_info pickling OK!"

def test_pickle_nullimporter():

    n1 = imp.NullImporter("...")
    s = pf.pickle_object(n1)
    n2 = pf.unpickle_object(s)
    assert type(n1) == type(n2)
    assert n2.find_module() == None

    print "imp.NullImporter pickling OK!"

def test_pickle_sys_singleton_namedtuples():

    o1 = sys.version_info
    s = pf.pickle_object(o1)
    o2 = pf.unpickle_object(s)
    assert o1 == o2

    o1 = sys.flags
    s = pf.pickle_object(o1)
    o2 = pf.unpickle_object(s)
    assert o1 == o2

    print "sys singleton named tuple pickling OK!"

def test_pickle_cell():

    def outer():
        a = 5
        def inner(num):
            return a + num
        return inner

    c1 = outer().__closure__
    s = pf.pickle_object(c1)
    c2 = pf.unpickle_object(s)
    assert c1 == c2

    print "Cell pickling OK!"

def test_pickle_member_descriptor():

    m1 = exceptions.SyntaxError.text
    assert repr(m1)[1:].startswith('member')
    s = pf.pickle_object(m1)
    m2 = pf.unpickle_object(s)
    assert m1 == m2

    class Subclass(exceptions.SyntaxError):
        pass

    m1 = Subclass.text
    assert repr(m1)[1:].startswith('member')
    s = pf.pickle_object(m1)
    m2 = pf.unpickle_object(s)
    assert m1 == m2

    print "Member descriptor pickling OK!"

def test_pickle_getset_descriptor():

    class TestClass(object):
        clsattr = 'Hello'
        def __init__(self):
            self.a = 1
            self.b = 2
            self.c = 3

    d1 = TestClass.__dict__['__dict__']
    assert repr(d1)[1:].startswith('attribute')
    s = pf.pickle_object(d1)
    d2 = pf.unpickle_object(s)
    assert type(d1) == type(d2)
    assert d1.__name__ == d2.__name__
    assert d1.__objclass__.__name__ == d2.__objclass__.__name__

    print "GetSet descriptor pickling OK!"

def test_pickle_file():

    for f in [sys.stdin, sys.stdout, sys.stderr]:
        f1 = f
        s = pf.pickle_object(f1)
        f2 = pf.unpickle_object(s)
        assert f1 == f2

    print "File pickling OK!"

def test_pickle_super():

    class Root(object):
        def foo(self):
            return "Root.foo"

    class Left(Root):
        def foo(self):
            return "Left.foo-" + super(Left, self).foo()

    class Right(Root):
        def foo(self):
            return "Right.foo-" + super(Right, self).foo()

    class Final(Left, Right):
        def foo(self):
            return "Final.foo-" + super(Final, self).foo()

    f = Final()

    s1 = super(Final, f)
    s = pf.pickle_object(s1)
    s2 = pf.unpickle_object(s)
    assert s1.foo() == s2.foo()

    s1 = super(Left, f)
    s = pf.pickle_object(s1)
    s2 = pf.unpickle_object(s)
    assert s1.foo() == s2.foo()

    s1 = super(Right, f)
    s = pf.pickle_object(s1)
    s2 = pf.unpickle_object(s)
    assert s1.foo() == s2.foo()

    print "Super pickling OK!"

def test_pickle_classmethod():

    class ClassMethodTestClass(object):
        @classmethod
        def test_cls_method(cls):
            return 'this is a %s class method' % str(cls)

        def test_instance(self):
            return 'this is a %s instance method' % str(self)

    c1 = classmethod(ClassMethodTestClass.test_cls_method)
    assert repr(c1)[1:].startswith("classmethod")
    s = pf.pickle_object(c1)
    c2 = pf.unpickle_object(s)
    assert c1.__func__.__name__ == c2.__func__.__name__

    cls1 = ClassMethodTestClass
    s = pf.pickle_object(cls1)
    cls2 = pf.unpickle_object(s)
    assert cls1.test_cls_method() == cls2.test_cls_method()

    print "Classmethod pickling OK!"

def test_pickle_wrapper_descriptor():

    d1 = object.__delattr__
    assert repr(d1)[1:].startswith("slot wrapper")
    s = pf.pickle_object(d1)
    d2 = pf.unpickle_object(s)
    assert d1 == d2

    print "Wrapper descriptor pickling OK!"

def test_pickle_method_wrapper():

    class TestClass(object): pass
    inst = TestClass()

    d1 = inst.__setattr__
    assert repr(d1)[1:].startswith("method-wrapper")
    s = pf.pickle_object(d1)
    d2 = pf.unpickle_object(s)

    assert d1.__name__ == d2.__name__
    d2('test', 123)
    assert d2.__self__.test == 123

    print "Method-wrapper (slot method) pickling OK!"

def test_pickle_range():

    r1 = xrange(1, 10, 2)
    s = pf.pickle_object(r1)
    r2 = pf.unpickle_object(s)
    assert repr(r1) == repr(r2)
    assert next(r1.__iter__()) == next(r2.__iter__())

    print "Range pickling OK!"

def test_pickle_slice():

    s1 = slice(2, 6, 2)
    s = pf.pickle_object(s1)
    s2 = pf.unpickle_object(s)
    assert repr(s1) == repr(s2)

    l = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]
    assert l[s2] == [2, 4]

    s1 = slice('Hello World', slice(1, 5), 123)
    s = pf.pickle_object(s1)
    s2 = pf.unpickle_object(s)
    assert repr(s1) == repr(s2)

    print "Slice pickling OK!"

def test_pickle_staticmethod():

    class TestClass(object):
        def __new__(cls):
            ret = object.__new__(cls)
            ret.myattr = 'Hello World'
            return ret
        @staticmethod
        def test_static_method(a):
            return "staticmethod with arg %s" % repr(a)

    s1 = staticmethod(TestClass.test_static_method)
    assert repr(s1)[1:].startswith('staticmethod')
    s = pf.pickle_object(s1)
    s2 = pf.unpickle_object(s)
    assert repr(s2)[1:].startswith('staticmethod')

    c1 = TestClass
    s = pf.pickle_object(c1)
    c2 = pf.unpickle_object(s)
    assert c1.test_static_method(123) == c2.test_static_method(123)

    inst = c2()
    assert inst.myattr == 'Hello World'
   
    print "staticmethod pickling OK!"

def test_pickle_buffer():

    text = 'Hello World'

    b1 = buffer(text, 6, 5)
    s = pf.pickle_object(b1)
    b2 = pf.unpickle_object(s)
    assert str(b2) == 'World'

    print "buffer pickling OK!"

def test_pickle_memoryview():

    bytes = bytearray('\x00\x01\x02\x03\x04\x05\x06\x07', 'UTF-8')

    m1 = memoryview(bytes)
    s = pf.pickle_object(m1)
    m2 = pf.unpickle_object(s)

    assert len(m1) == len(m2)
    for i in range(len(m1)):
        assert m1[i] == m2[i]

    print "memoryview pickling OK!"

def test_pickle_property():

    class PropertyClass(object):

        def __init__(self):
            self.__dict__['prop'] = 'default'

        @property
        def prop(self):
            return 'GET.' + self.__dict__['prop']

        @prop.setter
        def prop(self, val):
            self.__dict__['prop'] = 'SET.' + val

        @prop.deleter
        def prop(self):
            self.__dict__['prop'] = 'DEL.' + self.__dict__['prop']

    p1 = PropertyClass.prop
    assert repr(p1)[1:].startswith('property')
    s = pf.pickle_object(p1)
    p2 = pf.unpickle_object(s)

    obj = PropertyClass()
    val = p2.fget(obj)
    assert val == 'GET.default'
    p2.fset(obj, 'permafrost engine')
    val = p2.fget(obj)
    assert val == 'GET.SET.permafrost engine'

    print "property pickling OK!"

def test_pickle_listiter():

    l1 = iter([1,2,3])
    assert l1.next() == 1
    s = pf.pickle_object(l1)
    l2 = pf.unpickle_object(s)
    assert l2.next() == 2
    assert l2.next() == 3
    try:
        l2.next()
    except StopIteration:
        pass
    else:
        raise Exception("Unexpected item returned by iterator")

    l1 = iter([])
    s = pf.pickle_object(l1)
    l2 = pf.unpickle_object(s)
    try:
        l2.next()
    except StopIteration:
        pass
    else:
        raise Exception("Unexpected item returned by iterator")

    print "List iterator pickling OK!"

def test_pickle_enumerate():

    e1 = enumerate([0,1,9], 0)
    assert e1.next() == (0, 0)
    s = pf.pickle_object(e1)
    e2 = pf.unpickle_object(s)
    assert e2.next() == (1, 1)
    assert e2.next() == (2, 9)

    print "enumerate pickling OK!"

def test_pickle_float():

    f1 = 15934.234349
    s = pf.pickle_object(f1)
    f2 = pf.unpickle_object(s)
    assert f1 == f2

    print "Float pickling OK!"

def test_pickle_complex():

    c1 = complex(1.23223, 0.09833)
    s = pf.pickle_object(c1)
    c2 = pf.unpickle_object(s)
    assert c1 == c2

    print "Complex pickling OK!"

def test_pickle_dictproxy():

    class TestClass(object): 
        """ Test docstring """
        pass

    d1 = TestClass.__dict__
    assert repr(d1).startswith("dict_proxy")
    s = pf.pickle_object(d1)
    d2 = pf.unpickle_object(s)
    assert type(d1) == type(d2)
    assert d1['__doc__'] == d2['__doc__']

    print "dictproxy pickling OK!"

try:
    test_pickle_int()
    test_pickle_long()
    test_pickle_string()
    test_pickle_unicode()
    test_pickle_tuple()
    test_pickle_list()
    test_pickle_dict()
    test_pickle_set()
    test_pickle_frozenset()
    test_pickle_cfunction()
    test_pickle_code()
    test_pickle_function()
    test_pickle_class()
    test_pickle_instance()
    test_pickle_type()
    test_pickle_bool()
    test_pickle_bytearray()
    test_pickle_baseobject()
    test_pickle_notimplemented()
    test_pickle_ellipsis()
    test_pickle_nullimporter()
    test_pickle_syslonginfo()
    test_pickle_sysfloatinfo()
    test_pickle_sys_singleton_namedtuples()
    test_pickle_cell()
    test_pickle_member_descriptor()
    test_pickle_getset_descriptor()
    test_pickle_file()
    test_pickle_super()
    test_pickle_classmethod()
    test_pickle_wrapper_descriptor()
    test_pickle_method_wrapper()
    test_pickle_range()
    test_pickle_slice()
    test_pickle_staticmethod()
    test_pickle_buffer()
    test_pickle_memoryview()
    test_pickle_property()
    test_pickle_listiter()
    test_pickle_enumerate()
    test_pickle_float()
    test_pickle_complex()
    test_pickle_dictproxy()
except Exception as e:
    traceback.print_exc()
finally:
    pf.global_event(pf.SDL_QUIT, None)

