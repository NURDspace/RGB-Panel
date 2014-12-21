#!/usr/bin/python
#This script requires youtube-dl to function,
#as well as opencv and python-imaging.

from PIL import Image
import subprocess
import time
import random
import socket
import os.path
import sys
import re
import glob
import cv2

HOST = 'glam'
PORT = 5001

panel = {}
panel['width']=128
panel['height']=16
panel['pwm']=6

blank = Image.new('RGB',(128,16),'black')
count_frames = 5

def conv64to128(img):
    outim = Image.new('RGB',(128,16))
    limg = img.transpose(Image.ROTATE_90)
    rimg = img.transpose(Image.ROTATE_270)
    img1 = rimg.crop((0,0,32,16))
    img2 = limg.crop((0,32,32,48))
    img3 = rimg.crop((0,32,32,48))
    img4 = limg.crop((0,0,32,16))
    outim.paste(img1,(0,0))
    outim.paste(img2,(32,0))
    outim.paste(img3,(64,0))
    outim.paste(img4,(96,0))
    return outim

def stringImage(im):
    frame = [0 for x in range(panel['width']*panel['height']/2)]
    output = [frame[:] for x in range(panel['pwm'])]
    w,h = im.size
    for x in range(w):
        for y in range(h):
            colours = [0,0,0]
            r,g,b = list(im.getpixel((x,y)))
            colours[0] = int((r*panel['pwm'])/256) #Rescale to pwm depth
            colours[1] = int((g*panel['pwm'])/256)
            colours[2] = int((b*panel['pwm'])/256)
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
    return outstr

def play(url):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    if 'youtube' not in url:
       print ('Missing a youtube url')
       return
    match = re.search(r'v=([\w-]+)',url)
    if match is None:
        print('No video found. Please try and give me a single video...')
        return
    id = match.group(1)
    if not os.path.isfile('yt/%s.flv'%id):
        cli = ['youtube-dl','-f','5','-o','yt/%s.flv'%id,'http://www.youtube.com/watch?v=%s'%id]
        print('Downloading...')
        subprocess.call(' '.join(cli), shell=True)
    cap = cv2.VideoCapture('yt/%s.flv'%id)
    frame_time = 1.0/float(cap.get(5))
    print('Playing...')
    s.connect((HOST, PORT))
    try:
        frame_num = 0
        sleep_time = 0.0
        final_time = time.time()+ frame_time*count_frames
        while cap.isOpened():
            ret, frame = cap.read()
            if sleep_time < 0: #Running too slowly; skip to keep up
                if frame_time/(frame_time-sleep_time)>=random.random():
                    continue
            convert = cv2.cvtColor(frame,cv2.COLOR_BGR2RGB)
            img = Image.fromarray(convert)
            img = img.resize((64,32),Image.ANTIALIAS)
            img = conv64to128(img)
            img = img.convert('RGB').transpose(Image.ROTATE_180)
            strn = stringImage(img)
            if sleep_time > 0: #Running too quickly; sleep to slow down
                time.sleep(sleep_time)
            s.send(strn)
            frame_num = frame_num + 1
            if frame_num >= count_frames: #Check how accurate the timings are
                frame_num = 0
                time_diff = time.time()-final_time
                sleep_time = sleep_time-(time_diff / count_frames)
                final_time = time.time()+frame_time*count_frames
    except Exception, e:
        print('Error: '+str(e))
        pass
    s.send(stringImage(blank))
    s.send(stringImage(blank)) #final flip
    s.close()

play(sys.argv[1])
