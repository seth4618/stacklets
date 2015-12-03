set term png
set output "bm.png"
set datafile separator ","
data = '< sed -e s/\"//g bm.csv | tail -n +2 | cut -d "," -f6,3 | sort'
f(x) = a*x + b
fit f(x) data u 1:2 via a, b
title_f(a,b) = sprintf('f(x) = %.2fx + %.2f', a, b)
#plot data
plot data u 1:2 w l, f(x) t title_f(a,b)
