build_lib(
  LIBNAME flow-monitor
  SOURCE_FILES
    helper/big-brother-flow-monitor-helper.cc
    model/flow-classifier.cc
    model/big-brother-flow-monitor.cc
    model/flow-probe.cc
    model/ipv4-flow-classifier.cc
    model/ipv4-flow-probe.cc
    model/big-brother-flow-probe.cc
    model/ipv6-flow-classifier.cc
    model/ipv6-flow-probe.cc
  HEADER_FILES
    helper/flow-monitor-helper.h
    model/flow-classifier.h
    model/flow-monitor.h
    model/flow-probe.h
    model/ipv4-flow-classifier.h
    model/ipv4-flow-probe.h
    model/big-brother-flow-probe.h
    model/ipv6-flow-classifier.h
    model/ipv6-flow-probe.h
  LIBRARIES_TO_LINK ${libinternet}
)
