import sys
import site
from os.path import dirname, abspath
from textwrap import dedent
from skbuild import setup
import clingo


if not site.ENABLE_USER_SITE and "--user" in sys.argv[1:]:
    site.ENABLE_USER_SITE = True

clingopath = abspath(dirname(clingo.__file__))

setup(
    version = '1.0.0',
    name = 'clingo-xor',
    description = 'CFFI-based bindings to the clingo-xor solver.',
    long_description = dedent('''\
        This package allows for adding the clingo-xor propagator as a
        theory to clingo.

        It can also be used as a clingo-xor solver running:

            python -m clingoxor CLINGOXOR_ARGUMENTS
        '''),
    long_description_content_type='text/markdown',
    author = 'Roland Kaminski',
    author_email = 'kaminski@cs.uni-potsdam.de',
    license = 'MIT',
    url = 'https://github.com/potassco/clingo-xor',
    install_requires=[ 'cffi', 'clingo' ],
    cmake_args=[ '-DCLINGOXOR_MANAGE_RPATH=OFF',
                 '-DCLINGOXOR_USE_IMATH=ON',
                 '-DPYCLINGOXOR_ENABLE=pip',
                 '-DPYCLINGOXOR_INSTALL_DIR=libpyclingo-xor',
                 f'-DPYCLINGOXOR_PIP_PATH={clingopath}' ],
    packages=[ 'clingoxor' ],
    package_data={ 'clingoxor': [ 'py.typed', 'import__clingo-xor.lib', 'clingo-xor.h' ] },
    package_dir={ '': 'libpyclingo-xor' },
    python_requires=">=3.6"
)
