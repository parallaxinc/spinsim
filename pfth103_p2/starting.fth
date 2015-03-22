: STAR 42 EMIT ;
: MARGIN CR 30 SPACES ;
: BLIP MARGIN STAR ;
: STARS 0 DO STAR LOOP ;
: BAR MARGIN 5 STARS ;
: F BAR BLIP BAR BLIP BLIP CR ;
: MULT CR 11 1 DO DUP I * . LOOP DROP ;
: TABLE CR 11 1 DO I MULT LOOP ;
: TABLE1 CR 11 1 DO 11 1 DO I J * . LOOP CR LOOP ;
: DUB 32767 1 DO I . I +LOOP ;
: GREET ." Hello, I speak Forth " ;
: GIFT ." chocolate" ;
: GIVER ." Mum" ;
: THANKS CR ." Dear " GIVER ." ,"
 CR ." Thanks for the " GIFT ." . " ;
: EGGSIZE DUP 18 < IF ." reject " ELSE
 DUP 21 < IF ." small " ELSE
 DUP 24 < IF ." medium " ELSE
 DUP 27 < IF ." large " ELSE
 DUP 30 < IF ." extra large " ELSE
 ." error "
 THEN THEN THEN THEN THEN DROP ;
: FALSE 0 ;
: TRUE -1 ;
: TEST IF ." non-" THEN ." zero " ;
: /CHECK ?DUP IF / THEN ;
: UNCOUNT DROP 1 - ;
: max-int -1 1 rshift ;
: min-int max-int negate 1 - ;
: max-uint -1 ;
: OUTPUT-TEST
 ." YOU SHOULD SEE THE STANDARD GRAPHIC CHARACTERS:" CR
 41 BL DO I EMIT LOOP CR
 61 41 DO I EMIT LOOP CR
 127 61 DO I EMIT LOOP CR
 ." YOU SHOULD SEE 0-9 SEPARATED BY A SPACE:" CR
 9 1+ 0 DO I . LOOP CR
 ." YOU SHOULD SEE 0-9 (WITH NO SPACES):" CR
 57 1+ 48 DO I 0 SPACES EMIT LOOP CR
 ." YOU SHOULD SEE A-G SEPARATED BY A SPACE:" CR
 71 1+ 65 DO I EMIT SPACE LOOP CR
 ." YOU SHOULD SEE 0-5 SEPARATED BY TWO SPACES:" CR
 5 1+ 0 DO I 48 + EMIT 2 SPACES LOOP CR
 ." YOU SHOULD SEE TWO SEPARATE LINES:" CR
 ." LINE 1" CR ." LINE 2" CR
 ." YOU SHOULD SEE THE NUMBER RANGES OF SIGNED AND UNSIGNED NUMBERS:" CR
 ."   SIGNED: " MIN-INT . MAX-INT . CR
 ." UNSIGNED: " 0 . MAX-UINT U. CR
 ;

output-test
f
10 mult
table
table1
dub
greet
thanks cr
17 eggsize cr
22 eggsize cr
25 eggsize cr
28 eggsize cr
32 eggsize cr
0 test cr
1 test cr
