import os.path
import sys
import platform
import re

from distutils.core import setup
from distutils.extension import Extension

if __name__ == "__main__":
    include_dirs = [ ]
    libraries = []
    library_dirs = []
    extra_compile_args = ['-g','-O0']
    extra_link_args = []

    # Create the list of extension modules
    ext_modules = []
    # MPWide source
    mpwide_cpp = ['../MPWide.cpp', '../Socket.cpp']
    # SWIG wrapper
    swig_cpp = ['MPWide_wrap.cxx']
    # SWIG source
    swig_src = 'MPWide.i'
    # Do we need to swig it?
    if not os.path.exists(swig_cpp[0]) or os.path.getmtime(swig_cpp[0]) < os.path.getmtime(swig_src):
        cmd = 'swig -c++ -python %s' % swig_src
        print cmd
        os.system(cmd)
    
    generation_ext = Extension('_MPWide',
                               sources=mpwide_cpp + swig_cpp,
                               extra_compile_args=extra_compile_args,
                               include_dirs=include_dirs,
                               extra_link_args=extra_link_args,
                               library_dirs=library_dirs,
                               libraries=libraries,
                               )
    
    setup(name='MPWide',
          version='1.3',
          author='Derek Groen',
          author_email='d.groen@ucl.ac.uk',
          ext_modules=[generation_ext]
          )
