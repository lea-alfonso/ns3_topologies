# Set output format to PNG
set terminal png

# Plot Throughput
set output "topology-throughput-plot.png"
set title "Throughput vs. Time \n\nTrace Source Path: /Names/Emitter/Counter"
set xlabel "Time (Seconds)"
set ylabel "Throughput (Mbps)"
set key inside
set datafile missing "-nan"
plot "throughput-data.dat" using 1:2 title "Throughput" with linespoints

# Plot Mean Delay
set output "topology-mean-delay-plot.png"
set title "Mean Delay vs. Time \n\nTrace Source Path: /Names/Emitter/Counter"
set xlabel "Time (Seconds)"
set ylabel "Mean Delay (ms)"
plot "throughput-data.dat" using 1:3 title "Mean Delay" with linespoints

# Plot Last Packet Delay
set output "topology-last-packet-delay-plot.png"
set title "Last Packet Delay vs. Time \n\nTrace Source Path: /Names/Emitter/Counter"
set xlabel "Time (Seconds)"
set ylabel "Last Packet Delay (ms)"
plot "throughput-data.dat" using 1:4 title "Last Packet Delay" with linespoints

# Plot Mean Jitter
set output "topology-mean-jitter-plot.png"
set title "Mean Jitter vs. Time \n\nTrace Source Path: /Names/Emitter/Counter"
set xlabel "Time (Seconds)"
set ylabel "Mean Jitter (ms)"
plot "throughput-data.dat" using 1:5 title "Mean Jitter" with linespoints
