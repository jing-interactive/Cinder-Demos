set SRC=..\..\..\tracy
robocopy %SRC%\client client /MIR 
robocopy %SRC%\common common /MIR 
robocopy %SRC%\libbacktrace libbacktrace /MIR
robocopy %SRC%\ .