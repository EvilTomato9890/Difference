set terminal pngcairo size 1280,720
set output 'graphs/teylor_plot.png'
set grid
set xlabel 'x'
set ylabel 'f(x)'
plot \
'graphs/plot_data_0.dat' using 1:2 with lines title 'f(x)',\
'graphs/plot_data_1.dat' using 1:2 with lines title 'f1(x)',\
'graphs/plot_data_2.dat' using 1:2 with lines title 'f2(x)',\
'graphs/plot_data_3.dat' using 1:2 with lines title 'f3(x)',\
'graphs/plot_data_4.dat' using 1:2 with lines title 'f4(x)',\
'graphs/plot_data_5.dat' using 1:2 with lines title 'f5(x)',\

unset output
