all: img_denoiser.c
	mpicc -g img_denoiser.c -o img_denoiser -lm

run:
	mpiexec -n 5 ./img_denoiser

clean: 
	$(RM) img_denoiser
	$(RM) output.txt
