set title "Network Performance Over Time"
set xlabel "Time (s)"
set ylabel "Performance value"
set key outside
set grid
stats 'test_stats.dat'

# Define solid line styles for avg.Throughput and avg.meanDelay
set style line 1 lc rgb "#FF0000" lw 2 dashtype solid  # avg.Throughput (red)
set style line 2 lc rgb "#0000FF" lw 2 dashtype solid  # avg.meanDelay (blue)

# Define semitransparent line styles for other metrics
set style line 3 lc rgb "#00FF0080" lw 2 dashtype solid  # jitter (green, semi-transparent)
set style line 4 lc rgb "#80008080" lw 2 dashtype solid  # meanDelay (purple, semi-transparent)
set style line 5 lc rgb "#FFA50080" lw 2 dashtype solid  # throughput (orange, semi-transparent)


a = STATS_columns
b = STATS_columns-1

plot "test_stats.dat" using 1:a with lines ls 1 title 'avg.Throughput', \
     "test_stats.dat" using 1:b with lines ls 2 title 'avg.meanDelay'


replot for [i=STATS_columns-2:2:-3] "test_stats.dat" using 1:i with lines ls 3 notitle
replot for [i=STATS_columns-3:2:-3] "test_stats.dat" using 1:i with lines ls 4 notitle
replot for [i=STATS_columns-4:2:-3] "test_stats.dat" using 1:i with lines ls 5 notitle

# Loop to replot additional lines
#do for [i=2:3] {
#   print sprintf("Iteration i=%d", i)
#   jitter = STATS_columns - i
#   meanDelay = jitter - 1
#   throughput = meanDelay - 1
#
#   # Generate meaningful titles
#   jitter_title = sprintf("Flow %d jitter", i)
#   meanDelay_title = sprintf("Flow %d meanDelay", i)
#   throughput_title = sprintf("Flow %d throughput", i)
#
#   # Replot the additional lines, preserving previous plots
#   replot "test_stats.dat" using 1:jitter with lines title jitter_title, \
#          "test_stats.dat" using 1:meanDelay with lines title meanDelay_title, \
#          "test_stats.dat" using 1:throughput with lines title throughput_title
#


    
