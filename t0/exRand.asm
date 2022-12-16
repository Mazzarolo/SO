; programa para o SO
; imprime 10 numeros aleatorios

     CARGI 0      ; A = 0
     MVAX         ; X = A

loop MVXA
     LE 3         ; A = es[rand]
     ESCR 0       ; print A
     INCX         ; X++
     MVXA         ; A = X
     SUB iter     ; A = A - iter
     DESVNZ loop  ; if X != iter goto loop
     PARA         ; stop
iter VALOR 10     ; iter = 10