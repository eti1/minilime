#!/usr/bin/python
from matplotlib import pyplot as plt
from matplotlib import animation
import numpy as np
import sys

def load(f,n):
	ff = np.fromfile(f,np.int16,2*n)
	ff.resize(len(ff)/2,2)
	ff=ff.transpose()
	ff = ff[0] + 1j*ff[1]
	return ff

N = 512
x=np.arange(N)
fig = plt.figure()
line, = plt.plot([],[])

plt.xlim(0,N)
plt.ylim(-100,0)

def init():
	line.set_data([],[])
	return line,

def animate(i):
	s = load(sys.stdin, N)/4096
	y = np.fft.fftshift(20*np.log10(np.abs(np.fft.fft(s))/N))
	line.set_data(x,y)
	return line,

ani = animation.FuncAnimation(fig,animate,init_func=init,interval=2,repeat=False)
plt.show()
