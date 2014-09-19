#!/usr/bin/python
'''Example image transmit code. Usage: transmit.py <image>'''
from PIL import Image
import socket
import sys
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

HOST = 'palmerston'
PORT = 5001

panel = {}
panel['width']=128
panel['height']=16
panel['pwm']=6

frame = [0 for x in range(panel['width']*panel['height']/2)]
output = [frame[:] for x in range(panel['pwm'])]
im = Image.open(sys.argv[1])
#image scale to size
im = im.convert('RGB')
im = im.transpose(Image.ROTATE_180)

w,h = im.size
for x in range(w):
    for y in range(h):
       colours = [0,0,0]
       r,g,b = list(im.getpixel((x,y)))
       colours[0] = int(r*panel['pwm']/255) #Rescale to pwm depth
       colours[1] = int(g*panel['pwm']/255)
       colours[2] = int(b*panel['pwm']/255)
       offset = 0
       if y>=(panel['height']/2): #Because the memory structure needs 2x8 instead of 1x16, we duplicate that here.
           y = y - (panel['height']/2)
           offset=3
       for n, colour in enumerate(colours):
           for z in range(panel['pwm']):
               if z < colour:
                   output[z][x*panel['height']/2+y] = output[z][x*panel['height']/2+y] | 2**(n+offset)
newframe = 128 #1000000
newpwm = 64   #0100000
for n in range(panel['pwm']):
    output[n][0] = output[n][0] | newpwm
output[0][0] = output[0][0] | newframe
output = [i for o in output for i in o]
outstr = ''
for fr in output:
    outstr = outstr + chr(fr)
outstr = outstr+outstr #flip buffer needs a second frame.
s.connect((HOST, PORT))
s.send(outstr)
s.close()
