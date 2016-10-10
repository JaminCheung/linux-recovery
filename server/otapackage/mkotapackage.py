import os
import sys
import shutil
import argparse
from otapackage.lib import base, log, image, dev
from otapackage import config


class Maker(object):

    printer = None

    def __init__(self):
        pass

    def start(self):
        self.dev = dev.Device.get_object_from_file()
        if not self.dev:
            self.printer.error("parsing file %s" %(config.partition_file_name))
            os._exit(1)

        self.image = image.Image.get_object_from_file()
        if not self.image:
            self.printer.error("parsing file %s" %
                               (config.customize_file_name))
            os._exit(1)

        if not self.judge():
            self.printer.error("judging mapping image")
            os._exit(1)

        self.image.generate()

        self.dev.generate()
        self.pack()
        os._exit(0)

    def pack(self):
        packagecnt = image.Image.get_image_total_cnt()
        self.printer.debug("Total %d packages will be created" %
                      (packagecnt))
        if config.signature_flag:
            cipher_lib_path = os.path.realpath(
                "%s/%s" % (config.signature_home, config.signature_cipher_lib))
            keys_public_path = os.path.realpath(
                config.signature_rsa_public_key)
            keys_private_path = os.path.realpath(
                config.signature_rsa_private_key)
        orgdir = os.getcwd()
        os.chdir(config.Config.get_outputdir_path())
        for i in range(0, packagecnt+1):
            output_package = ("%s%03d") % (config.output_package_name, i)
            output_package_unencrypted = ("%s.unencrypted.zip" % (
                output_package))
            output_package_encrypted = ("%s.zip" % (output_package))
            caller_zip = "zip -r %s %s" % (
                output_package_unencrypted, output_package)
            os.system(caller_zip)
            if config.signature_flag:
                caller_signature ="java -jar -Xms512M -Xmx1024M %s -w %s %s %s %s" % (
                        cipher_lib_path, keys_public_path, keys_private_path,
                        output_package_unencrypted, output_package_encrypted)
                os.system(caller_signature)

            os.remove(output_package_unencrypted)
            shutil.rmtree(output_package)
        os.chdir(orgdir)
        return True

    # problem occured when multiple filesystem image is in one partittion
    # for example:  mtdblock3 <== (ubifs + others file system)
    def judge(self):
        dev = self.dev
        imageinfos = self.image.imageinfo
        for img in imageinfos:
            if not dev.judge_partititon_mapped(img.offset, img.size, img.type):
                self.printer.error("mapping image into partition")
                return False
        return True

    def cmdline_parse(self):
        parser = argparse.ArgumentParser()
        help = '''The Image path is where you place the images'''
        parser.add_argument('-i', '--imgpath', help=help)
        help = '''The output path is where you place the generated packages'''
        parser.add_argument('-o', '--output', help=help)
        help = '''The slice size is the length you pointed for slice updatemode.
                  Multiple is interger, base is %dMB.
                  Default is %dMB''' % (
            config.slicebase/1024/1024, config.slicesize/config.slicebase)
        parser.add_argument('-s', '--slicesize', help=help, type=int)
        help = '''Print debug information'''
        parser.add_argument('-v', '--verbose', help=help, action='store_true')

        if config.signature_flag:
            help = '''public key path'''
            parser.add_argument('--publickey', help=help)
            help = '''private key path'''
            parser.add_argument('--privatekey', help=help)

        args = parser.parse_args()
        if args.imgpath:
            config.Config.set_image_path(args.imgpath)
        if args.output:
            config.Config.set_outputdir_path(args.output)
        if args.slicesize:
            config.Config.set_slicesize(args.slicesize)
        if config.signature_flag:
            if args.publickey:
                config.Config.set_public_key(args.publickey)
            if args.privatekey:
                config.Config.set_private_key(args.privatekey)

        self.printer = log.Logger.get_logger(config.log_console)
        if args.verbose == True:
            log.Logger.set_level(config.log_console, 'd')

        
        self.printer.debug("command line parser")
        self.printer.debug("image configuration file path: %s" %
                      (config.Config.get_image_cfg_path()))
        self.printer.debug("image file path: %s" %
                      (config.Config.get_image_path()))
        self.printer.debug("output directory path: %s" %
                      (config.Config.get_outputdir_path()))
        self.printer.debug("slice size: %d" % (config.Config.get_slicesize()))
        self.printer.debug("public key path: %s" %
                      (config.Config.get_public_key()))
        self.printer.debug("private key path: %s" %
                      (config.Config.get_private_key()))
        return self


def main():
    Maker().cmdline_parse().start()

if __name__ == '__main__':
    main()
    pass
