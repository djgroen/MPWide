from distutils.core import setup
from Cython.Build import cythonize
from distutils.extension import Extension
from Cython.Distutils import build_ext

# load the shared object
from distutils.core import setup
from Cython.Build import cythonize

setup(
  cmdclass = {'build_ext': build_ext},
  ext_modules = cythonize(
           "MPWide.pyx",               # our Cython source
           language="c++",             # generate C++ code
      ))
