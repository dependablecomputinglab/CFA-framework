# CFA: Comprehensive Failure Analysis Framework
CFA is a a comprehensive failure analysis framework to find the microarchitectural components and software instructions that are most vulnerable to soft errors. This framework is based on the cycle-accurate system-level gem5 simulator [1]. CFA can inject soft-error-modeled faults into a microarchitectural
component, log their impacts on system behaviors, and analyze system failures from both hardware and software perspectives. For the hardware perspective analysis, CFA injects faults into specific microarchitectural components to estimate the vulnerability of each component separately. For the software perspective analysis, CFA performs the root cause instruction analysis, a novel technique to find the software instructions responsible for system failure.  

## References
[1] Binkert, N., Beckmann, B., Black, G., Reinhardt, S.K., Saidi, A., Basu, A., Hestness, J., Hower, D.R., Krishna, T., Sardashti, S., Sen, R., Sewell, K., Shoaib, M., Vaish, N., Hill, M.D.,Wood, D.A., 2011. The gem5 simulator. SIGARCH Computer Architecture News 39, 1–7.
