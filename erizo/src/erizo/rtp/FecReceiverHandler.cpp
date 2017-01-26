#include "rtp/FecReceiverHandler.h"
#include "./MediaDefinitions.h"
#include "./WebRtcConnection.h"

namespace erizo {

DEFINE_LOGGER(FecReceiverHandler, "rtp.FecReceiverHandler");

FecReceiverHandler::FecReceiverHandler(WebRtcConnection *connection) :
    connection_{connection}, enabled_{false} {
  fec_receiver_.reset(webrtc::UlpfecReceiver::Create(this));
}

void FecReceiverHandler::setFecReceiver(std::unique_ptr<webrtc::UlpfecReceiver>&& fec_receiver) {  // NOLINT
  fec_receiver_ = std::move(fec_receiver);
}

void FecReceiverHandler::enable() {
  enabled_ = true;
}

void FecReceiverHandler::disable() {
  enabled_ = false;
}

void FecReceiverHandler::write(Context *ctx, std::shared_ptr<dataPacket> packet) {
  if (enabled_ && packet->type == VIDEO_PACKET) {
    RtpHeader *rtp_header = reinterpret_cast<RtpHeader*>(packet->data);
    if (rtp_header->getPayloadType() == RED_90000_PT) {
      // This is a RED/FEC payload, but our remote endpoint doesn't support that
      // (most likely because it's firefox :/ )
      // Let's go ahead and run this through our fec receiver to convert it to raw VP8
      webrtc::RTPHeader hacky_header;
      hacky_header.headerLength = rtp_header->getHeaderLength();
      hacky_header.sequenceNumber = rtp_header->getSeqNumber();
      // FEC copies memory, manages its own memory, including memory passed in callbacks (in the callback,
      // be sure to memcpy out of webrtc's buffers
      if (fec_receiver_->AddReceivedRedPacket(hacky_header,
                            (const uint8_t*) packet->data, packet->length, ULP_90000_PT) == 0) {
        fec_receiver_->ProcessReceivedFec();
      }
    }
  }

  ctx->fireWrite(packet);
}

bool FecReceiverHandler::OnRecoveredPacket(const uint8_t* rtp_packet, size_t rtp_packet_length) {
  getContext()->fireWrite(std::make_shared<dataPacket>(0, (char*)rtp_packet, rtp_packet_length, VIDEO_PACKET));  // NOLINT
  return true;
}

int32_t FecReceiverHandler::OnReceivedPayloadData(const uint8_t* /*payload_data*/, size_t /*payload_size*/,
                                                const webrtc::WebRtcRTPHeader* /*rtp_header*/) {
    // Unused by WebRTC's FEC implementation; just something we have to implement.
    return 0;
}
}  // namespace erizo