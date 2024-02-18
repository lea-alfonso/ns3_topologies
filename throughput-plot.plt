set terminal png
set output "throughput-plot.png"
set title "Mean Throughput vs. Time \n\nTrace Source Path: /Names/Emitter/Counter"
set xlabel "Time (Seconds)"
set ylabel "Emitter Count"

set key inside
set datafile missing "-nan"
plot "throughput-data.dat" index 0 title "Emitter Count" with linespoints
