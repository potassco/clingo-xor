'''
This module provides a clingo.Theory class for a XOR theory.
'''

from clingo.theory import Theory
from ._clingoxor import lib as _lib, ffi as _ffi

__all__ = ['ClingoXORTheory']

class ClingoXORTheory(Theory):
    '''
    The DL theory.
    '''
    def __init__(self):
        super().__init__("clingoxor", _lib, _ffi)
