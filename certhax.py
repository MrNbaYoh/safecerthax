from mitmproxy import ctx
from mitmproxy import http
from mitmproxy import log
from mitmproxy.tools.console import eventlog
from mitmproxy.tools.console.window import WindowStack
from mitmproxy import command
from base64 import b64encode
from struct import pack

import sys

class CerthaxLog(eventlog.EventLog):
    title = "Certhax"
    keyctx = "certhaxlog"
    def __init__(self, master):
        super().__init__(master)
        master.options.console_focus_follow = True
    def add_event(self, event_store, entry: log.LogEntry):
        if not str(entry.msg).startswith("[certhax]"): return
        super().add_event(event_store, entry)

@command.command("console.view.certhaxlog")
def view_certhaxlog() -> None:
    ctx.master.switch_view("eventlog")


def load(l):
    for stack in ctx.master.window.stacks:
        stack.windows["certhaxlog"] = CerthaxLog(ctx.master)
    ctx.master.switch_view("certhaxlog")
    l.add_option(
        name = "certhax_payload",
        typespec = str,
        default = "",
        help = "Initial certhax payload"
    )
    l.add_option(
        name = "arm9_payload",
        typespec = str,
        default = "",
        help = "Arm9 payload sent just after the initial payload"
    )

certhax_payload_b64 = None
arm9_payload_b64 = None

def configure(updates):
    global certhax_payload_b64
    global arm9_payload_b64
    if "certhax_payload" in updates:
        if ctx.options.certhax_payload:
            try:
                file = open(ctx.options.certhax_payload, "rb")
                certhax_payload_b64 = b64encode(file.read()).decode("utf-8")
                file.close()
                ctx.log.info("[certhax] - Successfully loaded certhax payload '" + ctx.options.certhax_payload + "'")
            except IOError:
                ctx.options.certhax_payload = ""
                certhax_payload_b64 = None
                ctx.log.error("[certhax] - Could not open payload file: " + ctx.options.certhax_payload)
    if "arm9_payload" in updates:
        if ctx.options.arm9_payload:
            try:
                file = open(ctx.options.arm9_payload, "rb")
                arm9_payload_b64 = b64encode(file.read()).decode("utf-8")
                file.close()
                ctx.log.info("[certhax] - Successfully loaded arm9 payload '" + ctx.options.arm9_payload + "'")
            except IOError:
                ctx.options.arm9_payload = ""
                arm9_payload_b64 = None
                ctx.log.error("[certhax] - Could not open payload file: " + ctx.options.arm9_payload)

conntest = "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\"><html><head><title>HTML Page</title></head><body bgcolor=\"#FFFFFF\">This is test.html page</body></html>"

GetSystemTitleHashResponse = "<?xml version=\"1.0\" encoding=\"utf-8\"?><soapenv:Envelope xmlns:soapenv=\"http://schemas.xmlsoap.org/soap/envelope/\" xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"><soapenv:Body><GetSystemTitleHashResponse xmlns=\"urn:nus.wsapi.broadon.com\"><Version>1.0</Version><DeviceId></DeviceId><MessageId></MessageId><TimeStamp></TimeStamp><ErrorCode>0</ErrorCode><TitleHash>FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF</TitleHash></GetSystemTitleHashResponse></soapenv:Body></soapenv:Envelope>"

GetAccountStatusResponse = "<?xml version=\"1.0\" encoding=\"utf-8\"?><soapenv:Envelope xmlns:soapenv=\"http://schemas.xmlsoap.org/soap/envelope/\" xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"><soapenv:Body><GetAccountStatusResponse xmlns=\"urn:ecs.wsapi.broadon.com\"><Version>2.0</Version><DeviceId></DeviceId><MessageId>1</MessageId><TimeStamp></TimeStamp><ErrorCode>0</ErrorCode><ServiceStandbyMode>false</ServiceStandbyMode><AccountStatus>R</AccountStatus><ServiceURLs><Name>ContentPrefixURL</Name><URI>http://ccs.cdn.c.shop.nintendowifi.net/ccs/download</URI></ServiceURLs><ServiceURLs><Name>UncachedContentPrefixURL</Name><URI>https://ccs.c.shop.nintendowifi.net/ccs/download</URI></ServiceURLs><ServiceURLs><Name>SystemContentPrefixURL</Name><URI>http://nus.cdn.c.shop.nintendowifi.net/ccs/download</URI></ServiceURLs><ServiceURLs><Name>SystemUncachedContentPrefixURL</Name><URI>https://ccs.c.shop.nintendowifi.net/ccs/download</URI></ServiceURLs><ServiceURLs><Name>EcsURL</Name><URI>https://ecs.c.shop.nintendowifi.net/ecs/services/ECommerceSOAP</URI></ServiceURLs><ServiceURLs><Name>IasURL</Name><URI>https://ias.c.shop.nintendowifi.net/ias/services/IdentityAuthenticationSOAP</URI></ServiceURLs><ServiceURLs><Name>CasURL</Name><URI>https://cas.c.shop.nintendowifi.net/cas/services/CatalogingSOAP</URI></ServiceURLs><ServiceURLs><Name>NusURL</Name><URI>https://nus.c.shop.nintendowifi.net/nus/services/NetUpdateSOAP</URI></ServiceURLs></GetAccountStatusResponse></soapenv:Body></soapenv:Envelope>"

GetSystemUpdateResponse = "<?xml version=\"1.0\" encoding=\"UTF-8\"?><soapenv:Envelope xmlns:soapenv=\"http://schemas.xmlsoap.org/soap/envelope/\" xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"><soapenv:Body><GetSystemUpdateResponse xmlns=\"urn:nus.wsapi.broadon.com\"><Version>1.0</Version><DeviceId></DeviceId><MessageId>1</MessageId><TimeStamp></TimeStamp><ErrorCode>0</ErrorCode><ContentPrefixURL>http://nus.cdn.c.shop.nintendowifi.net/ccs/download</ContentPrefixURL><UncachedContentPrefixURL>https://ccs.c.shop.nintendowifi.net/ccs/download</UncachedContentPrefixURL><TitleVersion><TitleId>00040010FFFFFFFF</TitleId><Version>0</Version><FsSize>262144</FsSize><TicketSize>848</TicketSize><TMDSize>4660</TMDSize></TitleVersion><UploadAuditData>1</UploadAuditData><TitleHash>FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF</TitleHash></GetSystemUpdateResponse></soapenv:Body></soapenv:Envelope>"

GetSystemCommonETicketResponse = "<?xml version=\"1.0\" encoding=\"utf-8\"?><soapenv:Envelope xmlns:soapenv=\"http://schemas.xmlsoap.org/soap/envelope/\" xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"><soapenv:Body><GetSystemCommonETicketResponse xmlns=\"urn:nus.wsapi.broadon.com\"><Version>1.0</Version><DeviceId></DeviceId><MessageId>1</MessageId><TimeStamp></TimeStamp><ErrorCode>0</ErrorCode><CommonETicket>AAAA</CommonETicket><Certs>{}</Certs><Certs>AAAA</Certs><Certs>AAAA</Certs><Certs>AAAA</Certs><Certs>{}</Certs></GetSystemCommonETicketResponse></soapenv:Body></soapenv:Envelope>"

def request(flow: http.HTTPFlow) -> None:
    if flow.request.url == "https://nus.c.shop.nintendowifi.net/nus/services/NetUpdateSOAP":
        if not ctx.options.certhax_payload or not certhax_payload_b64:
            ctx.log.warn("[certhax] - The certhax payload file is not set, the exploit won't be triggered...")
            return
        if not ctx.options.arm9_payload or not arm9_payload_b64:
            ctx.log.warn("[certhax] - The arm9 payload file is not set, the exploit won't be triggered...")
            return
        if b"nus:GetSystemTitleHash" in flow.request.content:
            ctx.log.info("[certhax] - Found nus:GetSystemTitleHash message")
            ctx.log.info("[certhax] - Sending dummy SystemTitleHash to trigger sysupdate attempt...")
            flow.response = http.HTTPResponse.make(
                200,
                GetSystemTitleHashResponse,
                {"Content-Type": "text/xml"}
            )
        elif b"nus:GetSystemUpdate" in flow.request.content:
            ctx.log.info("[certhax] - Found nus:GetSystemUpdate message")
            ctx.log.info("[certhax] - Sending dummy TitleId to trigger ticket installation attempt...")
            flow.response = http.HTTPResponse.make(
                200,
                GetSystemUpdateResponse,
                {"Content-Type": "text/xml"}
            )
        elif b"nus:GetSystemCommonETicket" in flow.request.content:
            ctx.log.info("[certhax] - Found nus:GetSystemCommonETicket message")
            ctx.log.info("[certhax] - Sending payload Certs to trigger the exploit...")
            flow.response = http.HTTPResponse.make(
                200,
                GetSystemCommonETicketResponse.format(certhax_payload_b64, arm9_payload_b64),
                {"Content-Type": "text/xml"}
            )
    elif flow.request.url == "https://ecs.c.shop.nintendowifi.net/ecs/services/ECommerceSOAP":
        if not ctx.options.certhax_payload or not certhax_payload_b64:
            ctx.log.warn("[certhax] - The certhax payload file is not set, the exploit won't be triggered...")
            return
        if not ctx.options.arm9_payload or not arm9_payload_b64:
            ctx.log.warn("[certhax] - The arm9 payload file is not set, the exploit won't be triggered...")
            return
        if b"ecs:GetAccountStatus" in flow.request.content:
            ctx.log.info("[certhax] - Found ecs:GetAccountStatus message")
            ctx.log.info("[certhax] - Sending list of services URL...")
            flow.response = http.HTTPResponse.make(
                200,
                GetAccountStatusResponse,
                {"Content-Type": "text/xml"}
            )
    elif flow.request.url == "http://conntest.nintendowifi.net/":
        ctx.log.info("[certhax] - Found conntest request")
        ctx.log.info("[certhax] - Sending response...")
        flow.response = http.HTTPResponse.make(
            200,
            conntest,
            {"Content-Type": "text/html", "X-Organization": "Nintendo"}
        )
