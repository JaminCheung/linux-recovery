#!/usr/bin/python
import argparse
import os
import sys
from otapackage.lib import base
from otapackage.depmod import image
from otapackage import config


class CustomizeImage(object):
    file = config.customize_file_name
    types = config.img_types
    updatemodes = config.updatemodes

    imgcnt = 0
    devctl = 0
    imageinfos = []

    @classmethod
    def get_default(cls):
        return cls()

    def generate_output(self):
        buf = "[update]\n"
        buf += "imgcnt=%d\n" % (self.imgcnt)
        if self.devctl:
            buf += "devctl=%d\n" % (self.devctl)
        for i in range(1, self.imgcnt+1):
            buf += "[image%d]\n" % (i)
            buf += "name=%s\n" % (self.imageinfos[i-1].name)
            buf += "type=%s\n" % (self.imageinfos[i-1].type)
            buf += "offset=%s\n" % (self.imageinfos[i-1].offset)
            buf += "updatemode=%s\n" % (self.imageinfos[i-1].updatemode)
        # print buf
        return buf

    def create_file_descript(self, filepath):
        buf = ''
        filename = '%s/%s' % (filepath, self.file)
        fd = open(filename, 'w')
        buf = self.generate_output()
        fd.write(buf)
        fd.close()
        return True

    @classmethod
    def cmdline_parse(cls):
        parser = argparse.ArgumentParser()
        help = '''The total count of images you want to update, 
                must be number but not character'''
        parser.add_argument('imgcnt', help=help, type=int)
        help = '''
        The special flag for device control. Optional value is listed as follows. 
            --devctl=1: Chip erase will be issued fisrtly before the update main sequence run
       '''
        parser.add_argument('-c', '--devctl', help=help, type=int)
        args = parser.parse_args()
        if not args.devctl:
            cls.imgcnt = args.imgcnt

        for i in range(1, int(args.imgcnt)+1):
            imgname = raw_input('image%d name: ' % i)
            imgtype = raw_input(
                'image%d type [must be one in %s]: ' % (i, cls.types))
            assert imgtype in cls.types, "image type '%s' is not in %s" % (
                imgtype, cls.types)
            imgoffset = input('image%d offset [hexadecimal is better]: ' % (i))
            updatemode = raw_input(
                'image%d updatemode [must be one in %s]: ' % (
                    i, cls.updatemodes))
            assert updatemode in cls.updatemodes, '''image updatemode '%s' 
                            is not in %s''' % (updatemode, cls.updatemodes)

            imageinfo = image.Image.Imageinfo(
                imgname, 0, imgtype, hex(imgoffset), updatemode)
            cls.imageinfos.append(imageinfo)

        cls.imgcnt = args.imgcnt


def main():
    CustomizeImage.cmdline_parse()
    filepath = config.customer_path
    CustomizeImage.get_default().create_file_descript(filepath)

if __name__ == '__main__':
    main()
