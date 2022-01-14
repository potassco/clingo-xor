dom(1..100).

{ p(X) } :- dom(X).

even(0).
even(X) :- not even(X-1),     p(X), dom(X).
even(X) :-     even(X-1), not p(X), dom(X).

odd(X) :- not odd(X-1),     p(X), dom(X).
odd(X) :-     odd(X-1), not p(X), dom(X).

:- dom(X), not dom(X+1), not even(X).
:- dom(X), not dom(X+1), not odd(X).
