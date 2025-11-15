All the code that is pertinent to this work is in 'sample'
The library that the code in 'sample' uses the most is 'include/nndll.h'

Furuno_Sample is the main entry point to the program
Web_Server is the primary user intereface to the Furuno_sample
Web_server will call pattern_contorl in a new thread when needed. 
stb_image_write is the function that FurunoSample uses to create pngs. 

data and frames directopries are created during run time to hold these output files from the program.