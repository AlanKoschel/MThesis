# Copyright 2017 Nitish Mutha (nitishmutha.com)

# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at

#     http://www.apache.org/licenses/LICENSE-2.0

# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from math import pi
import numpy as np
import sys

ENABLE_GRAPH_ANALYSIS = 1

class NFOV():
    def __init__(self, height=800, width=1600):
        self.FOV = [0.65, 0.38] # human eyese hor = 110° and vert = 70°
        self.PI = pi * 0.5 # 180°
        self.PI_2 = pi * 0.5
        self.PI2 = pi * 2.0
        self.height = height
        self.width = width
        self.screen_points = self._get_screen_img()

    def _get_coord_rad(self, isCenterPt, center_point=None):
        return (center_point * 2 - 1) * np.array([self.PI, self.PI_2]) \
            if isCenterPt \
            else \
            (self.screen_points * 2 - 1) * np.array([self.PI, self.PI_2]) * ( 
                np.ones(self.screen_points.shape) * self.FOV) # otherwise return this

    def _get_screen_img(self):
        xx, yy = np.meshgrid(np.linspace(0, 1, self.width), np.linspace(0, 1, self.height))
        return np.array([xx.ravel(), yy.ravel()]).T # puts the 2-dimensional arrays into 1-dimension

    def _calcSphericaltoGnomonic(self, convertedScreenCoord):
        x = convertedScreenCoord.T[0]
        y = convertedScreenCoord.T[1]

        rou = np.sqrt(x ** 2 + y ** 2)
        c = np.arctan(rou)
        sin_c = np.sin(c)
        cos_c = np.cos(c)

        lat = np.arcsin(cos_c * np.sin(self.cp[1]) + (y * sin_c * np.cos(self.cp[1])) / rou)
        lon = self.cp[0] + np.arctan2(x * sin_c, rou * np.cos(self.cp[1]) * cos_c - y * np.sin(self.cp[1]) * sin_c)
        
        lat = (lat / self.PI_2 + 1.) * 0.5
        lon = (lon / self.PI + 1.) * 0.5
        
        if ENABLE_GRAPH_ANALYSIS:
            print("longitude:", lon)
            print("latitude:", lat)
            plt.figure("lat long coordinates in desired picture")
            plt.plot(lon, lat)
            axes = plt.gca()
            axes.set_xlim([0, 1])
            axes.set_ylim([0,1])
        
        return np.array([lon, lat]).T

    def _bilinear_interpolation(self, screen_coord):
        # calc pixel position from selected view in image
        uf = np.mod(screen_coord.T[0],1) * self.frame_width  # long - width
        vf = np.mod(screen_coord.T[1],1) * self.frame_height  # lat - height
        
        if ENABLE_GRAPH_ANALYSIS:
            plt.figure("1: lat long projected on original image")
            plt.plot(uf, vf)
            axes = plt.gca()
            axes.set_xlim([0, self.frame_width])
            axes.set_ylim([0,self.frame_height])
        
        x0 = np.floor(uf).astype(int)  # coord of pixel to bottom left
        y0 = np.floor(vf).astype(int)
        x2 = np.add(x0, np.ones(uf.shape).astype(int))  # coords of pixel to top right
        y2 = np.add(y0, np.ones(vf.shape).astype(int))
        
        if ENABLE_GRAPH_ANALYSIS:
            print("x2:", x2)
            print("y2:", y2)
            plt.figure("2: lat long projected on original image")
            plt.plot(x2, y2)
            axes = plt.gca()
            axes.set_xlim([0, self.frame_width])
            axes.set_ylim([0,self.frame_height])

        base_y0 = np.multiply(y0, self.frame_width)
        base_y2 = np.multiply(y2, self.frame_width)
        
        if ENABLE_GRAPH_ANALYSIS:
            print("base_y2:", base_y2)
            plt.figure("3: lat long projected on original image")
            plt.plot(x2, base_y2)
            axes = plt.gca()

        A_idx = np.add(base_y0, x0)
        B_idx = np.add(base_y2, x0)
        C_idx = np.add(base_y0, x2)
        D_idx = np.add(base_y2, x2)
        flat_img = np.reshape(self.frame, [-1, self.frame_channel])
        
        if ENABLE_GRAPH_ANALYSIS:
            print("base_y2 + x2:", D_idx)
            plt.figure("4: lat long projected on original image")
            plt.plot(x2, D_idx)
            axes = plt.gca()
        
        print(flat_img.shape)
        A = np.take(flat_img, A_idx, axis=0)
        B = np.take(flat_img, B_idx, axis=0)
        C = np.take(flat_img, C_idx, axis=0)
        D = np.take(flat_img, D_idx, axis=0)
        if ENABLE_GRAPH_ANALYSIS:
            nfov = np.reshape(np.round(A + B + C + D).astype(np.uint8), [self.height, self.width, 3])
            plt.figure()
            plt.imshow(nfov)
            
        wa = np.multiply(x2 - uf, y2 - vf)
        wb = np.multiply(x2 - uf, vf - y0)
        wc = np.multiply(uf - x0, y2 - vf)
        wd = np.multiply(uf - x0, vf - y0)

        # interpolate
        AA = np.multiply(A, np.array([wa, wa, wa]).T)
        BB = np.multiply(B, np.array([wb, wb, wb]).T)
        CC = np.multiply(C, np.array([wc, wc, wc]).T)
        DD = np.multiply(D, np.array([wd, wd, wd]).T)
        nfov = np.reshape(np.round(AA + BB + CC + DD).astype(np.uint8), [self.height, self.width, 3])
       
        im.imwrite('C:\\1_GitAlan\\nfov.jpg', nfov)
        if ENABLE_GRAPH_ANALYSIS:
            plt.figure()
            plt.imshow(nfov)
            plt.show()
        
        return nfov

    def toNFOV(self, frame, center_point):
        self.frame = frame
        self.frame_height = frame.shape[0]
        self.frame_width = frame.shape[1]
        self.frame_channel = frame.shape[2]

        self.cp = self._get_coord_rad(center_point=center_point, isCenterPt=True)
        convertedScreenCoord = self._get_coord_rad(isCenterPt=False)
        spericalCoord = self._calcSphericaltoGnomonic(convertedScreenCoord)
        return self._bilinear_interpolation(spericalCoord)


# test the class
if __name__ == '__main__':
    
    import imageio as im
    if ENABLE_GRAPH_ANALYSIS:
        import matplotlib.pyplot as plt
        np.set_printoptions(precision=2)
        
    img = im.imread(sys.argv[1])
    nfov = NFOV()
    center_point = np.array([0.5, 0.5])  # camera center point (valid range [0,1])
    nfov.toNFOV(img, center_point)
