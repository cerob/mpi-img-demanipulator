all: img_denoiser.c
	mpicc -g img_denoiser.c -o img_denoiser -lm

run:
	time mpiexec -n 5 ./img_denoiser yinyang_noisy.txt output.txt 0.4 0.2

show:
	time mpiexec -n 5 ./img_denoiser yinyang_noisy.txt output.txt 0.4 0.2
	../scripts/env/bin/python ../scripts/text_to_image.py output.txt output.jpg

clean: 
	$(RM) img_denoiser
	$(RM) output.txt
	$(RM) output.jpg
