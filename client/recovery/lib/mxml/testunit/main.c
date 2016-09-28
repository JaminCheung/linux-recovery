#include <utils/log.h>
#include <utils/list.h>
#include <lib/mxml/mxml.h>

#define LOG_TAG "test_libmxml"

#define XML_FILE "update.xml"

int main(void) {
    LOGD("========== TEST Libmxml ==========\n");

    FILE* fp;

    mxml_node_t *xml;           // <?xml ... ?>
    mxml_node_t *devctl;        // <devctl>
    mxml_node_t *update;        // <update>
    mxml_node_t *image_list;    // <imagelist>
    mxml_node_t *image;

    fp = fopen(XML_FILE, "r");
    if (fp == NULL) {
        LOGE("Failed to open %s: %s", XML_FILE, strerror(errno));
        return -1;
    }

    xml = mxmlLoadFile(NULL, fp, MXML_TEXT_CALLBACK);
    fclose(fp);

    if (xml == NULL) {
        LOGE("Failed to load %s\n", XML_FILE);
        return -1;
    }

    devctl = mxmlFindElement(xml, xml, "devctl", NULL, NULL, MXML_DESCEND);
    if (devctl == NULL) {
        LOGE("Failed to find element devctl\n");
        return -1;
    }

    LOGD("devctl=%s", mxmlGetText(devctl, 0));

    return 0;
}
