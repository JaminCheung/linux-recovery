from otapackage.lib import base
import sys

#### Info for configuration file ####
# partition file contain a partition table for specified device
partition_file_name = 'partition.conf'
# customize file contain descriptions for images to be update
customize_file_name = 'customization.conf'
# where is partition.conf/customize.conf deployed
customer_path = 'otapackage/customer/generated/'

#### Info for package making ####
# where is image files deployed to be sliced
image_path = 'otapackage/res/image'
# where is update files deployed
output_path = 'otapackage/out'

# log
log_size = 10 * 1024 * 1024
log_prompt = "otapackage"
log_file = "file"
log_console = "console"
log_logname = ".otapackage_log"
log_backup_cnt = 5
# signature
# whether signature package or not
signature_flag = True
signature_home = 'otapackage/depmod/signature'
signature_cipher_lib = 'signapk/signapk.jar'
signature_key_dir = 'otapackage/res/keys'
signature_rsa_public_key = "%s/%s" % (signature_key_dir, "testkey.x509.pem")
signature_rsa_private_key = "%s/%s" % (signature_key_dir, "testkey.pk8")

# generated packages for ota update application
# package named with prefix 'update', fullname is 'update'+'X'+'.zip', X
# will be superseded by specific number, maximum is the slice count
# update configuration name called 'update.xml' which is a desctiption
# file containing all the update infomation of packages
output_package_name = "update"
output_config_name = "update.xml"
output_partition_name = "device.xml"
output_pack_config_index = 0
output_pack_config_dir = "%s%03d" % (
    output_package_name, output_pack_config_index)
xml_encoding = "utf-8"
xml_declaration = True
xml_data_type_string = "opaque"
xml_data_type_integer = "integer"
# device type supported by predefine
device_types = ('nor', 'nand', 'mmc')
# enum defination relative to device_types
enum_device_types = base.enum_f2('nor', 'nand', 'mmc')
# default type is enum, optional value is listed below
# devctls = base.enum_f2('none', 'eraseall')
devctls = {'none': 0, 'eraseall': 1}
# image file type supported by system define
img_types = ('normal', 'ubifs', 'jffs2', 'cramfs', 'yaffs2')
# enum defination relative to img_types
e_img_types = base.enum_f1(
    normal=0, ubifs=0x110, jffs2=0x111, cramfs=0x112, yaffs2=0x113)
# update mode supported by system defined
updatemodes = ('full', 'slice')
# enum defination relative to updatemodes
# e_updatemodes = base.enum_f1(full=0x200, slice=0x201)
e_updatemodes = {'full': 0x200, 'slice': 0x201}
# slice chunk size, unit is byte
slicesize = 1024*1024
slicebase = 1024*1024

# system preset value, if there is no abundent reason, you should not
# change them
# (Backward Compatibility)
nandflash_page_size = 2048
nandflash_pages_per_block = 64
nandflash_block_size = nandflash_page_size * nandflash_pages_per_block
# ubifs
ubi_layout_volume_ebs = 2
ubi_wl_reserved_pebs = 1
ubi_eba_reserved_pebs = 1
ubi_mtd_ubi_beb_limit_per1024 = 20
#(Backward Compatibility)
ubi_leb_size = nandflash_page_size * (nandflash_pages_per_block - 2)
# yaffs2
yaffs2_tagsize_per_page = 28
yaffs2_page_size = (nandflash_page_size+yaffs2_tagsize_per_page)
yaffs2_block_size = yaffs2_page_size * nandflash_pages_per_block
local = locals()


class Config(object):
    v = {
        'image_cfg_path': customer_path,
        'image_path': image_path,
        'outputdir_path': output_path,
        'slicesize': slicesize,
        'public_key': signature_rsa_public_key,
        'private_key': signature_rsa_private_key,
    }
    # where is customized image configuration

    @classmethod
    def get_image_cfg_path(cls):
        return cls.v['image_cfg_path']

    @classmethod
    def set_image_cfg_path(cls, path):
        cls.v['image_cfg_path'] = path

    # where is image repository
    @classmethod
    def get_image_path(cls):
        return cls.v['image_path']

    @classmethod
    def set_image_path(cls, path):
        cls.v['image_path'] = path

    # where is update package
    @classmethod
    def get_outputdir_path(cls):
        return cls.v['outputdir_path']

    @classmethod
    def set_outputdir_path(cls, path):
        cls.v['outputdir_path'] = path

    # get each slice size for slice updatemode
    @classmethod
    def get_slicesize(cls):
        return cls.v['slicesize']

    @classmethod
    def set_slicesize(cls, size):
        cls.v['slicesize'] = size

    # public key path
    @classmethod
    def get_public_key(cls):
        return cls.v['public_key']

    @classmethod
    def set_public_key(cls, key):
        cls.v['public_key'] = key

    # private key path
    @classmethod
    def get_private_key(cls):
        return cls.v['private_key']

    @classmethod
    def set_private_key(cls, key):
        cls.v['private_key'] = key
