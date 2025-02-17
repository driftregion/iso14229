#include <CAN.h>
#include "iso14229.h"
#include <stdarg.h>
#include <stdint.h>

static UDSServer_t srv;
static UDSISOTpC_t tp;

extern "C" uint32_t isotp_user_get_us(void) { return UDSMillis() * 1000; }

extern "C" int isotp_user_send_can(uint32_t arb_id, const uint8_t *data, const uint8_t size, void *ud) {
  (void)ud;
  CAN.beginPacket(arb_id);
  CAN.write(data, size);
  CAN.endPacket();
  return size;
}

extern "C" void isotp_user_debug(const char *fmt, ...) {
  (void)fmt;
}

static void CANRecv(UDSISOTpC_t *tp) {
    assert(tp);
    uint8_t buf[8];
    int len = CAN.parsePacket();
    if (len) {
        if (len > 8) {
            Serial.println("CAN packet too long, truncating");
            len = 8;
        }
        CAN.readBytes(buf, len);
        UDS_LOGI(__FILE__, "can recv\n");
        if (CAN.packetId() == tp->phys_sa) {
          UDS_LOGI(__FILE__, "phys frame received\n");
          isotp_on_can_message(&tp->phys_link, buf, len);
        } else if (CAN.packetId() == tp->func_sa) {
          if (ISOTP_RECEIVE_STATUS_IDLE != tp->phys_link.receive_status) {
            UDS_LOGI(__FILE__, "func frame received but cannot process because link is not idle");
            return;
          }
          isotp_on_can_message(&tp->func_link, buf, len);
    }
  }
}

static UDSErr_t fn(UDSServer_t *srv, UDSEvent_t ev, void *arg) {
    Serial.print("Got event ");
    Serial.print(ev);
    Serial.print(", (");
    Serial.print(UDSEventToStr(ev));
    Serial.println(")");

    switch (ev) {
    case UDS_EVT_Err: {
        UDSErr_t *p_err = (UDSErr_t *)arg;
        Serial.print("Err: ");
        Serial.println(*p_err);
        break;
    }

    case UDS_EVT_EcuReset:
        Serial.println("EcuReset");
        return UDS_OK;

    case UDS_EVT_DoScheduledReset:
        NVIC_SystemReset();   // Perform a system reset
        return UDS_OK;

    default:
        printf("Unhandled event %s (%d)\n", UDSEventToStr(ev), ev);
        return UDS_NRC_ServiceNotSupported;
    }
    return UDS_NRC_ServiceNotSupported;
}

void setup() {
  Serial.begin(9600);

  UDSErr_t err = UDSServerInit(&srv);
  if(UDS_OK != err) {
    Serial.print("UDSServerInit failed with err: ");
    Serial.println(UDSErrToStr(err));
    while(1);
  }

  const UDSISOTpCConfig_t tp_cfg = {
      .source_addr=0x7E8,
      .target_addr=0x7E0,
      .source_addr_func=0x7DF,
      .target_addr_func=UDS_TP_NOOP_ADDR,
  };

  err = UDSISOTpCInit(&tp, &tp_cfg);
  if (UDS_OK != err) {
    Serial.print("UDSISOTpCInit failed with err: ");
    Serial.println(UDSErrToStr(err));
    while(1);
  }
  
  srv.tp = &tp.hdl;
  srv.fn = fn;

  // start the CAN bus at 500 kbps
  if (!CAN.begin(500E3)) {
    Serial.println("Starting CAN failed!");
    while (1);
  }

  Serial.println("Arduino UDS Server: Setup Complete");
}

void loop() {
  UDSServerPoll(&srv);
  CANRecv(&tp);
}
