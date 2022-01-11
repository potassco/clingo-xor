{ x; y }.

&sum { x; y } >= 1.

&sum { x } >= 1 :-     x.
&sum { x } <= 0 :- not x.

&sum { y } >= 1 :-     y.
&sum { y } <= 0 :- not y.
