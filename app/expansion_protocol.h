/**
 * @file expansion_protocol.h
 * @brief Flipper Expansion Protocol parser reference implementation.
 *
 * This file is written with low-spec hardware in mind. It does not use
 * dynamic memory allocation or Flipper-specific libraries and can be
 * included directly into any module's firmware's sources.
 */
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Maximum data size per frame, in bytes.
 */
#define EXPANSION_MAX_DATA_SIZE (64U)

/**
 * @brief Enumeration of supported frame types.
 */
typedef enum {
    ExpansionFrameTypeHeartbeat = 1, /**< Heartbeat frame. */
    ExpansionFrameTypeStatus = 2, /**< Status report frame. */
    ExpansionFrameTypeBaudRate = 3, /**< Baud rate negotiation frame. */
    ExpansionFrameTypeControl = 4, /**< Control frame. */
    ExpansionFrameTypeData = 5, /**< Data frame. */
    ExpansionFrameTypeReserved, /**< Special value. */
} ExpansionFrameType;

/**
 * @brief Enumeration of possible error types.
 */
typedef enum {
    ExpansionFrameErrorNone = 0x00, /**< No error occurred. */
    ExpansionFrameErrorUnknown = 0x01, /**< An unknown error has occurred (generic response). */
    ExpansionFrameErrorBaudRate = 0x02, /**< Requested baud rate is not supported. */
} ExpansionFrameError;

/**
 * @brief Enumeration of suported control commands.
 */
typedef enum {
    ExpansionFrameControlCommandStartRpc = 0x00, /**< Start an RPC session. */
    ExpansionFrameControlCommandStopRpc = 0x01, /**< Stop an open RPC session. */
} ExpansionFrameControlCommand;

#pragma pack(push, 1)

/**
 * @brief Frame header structure.
 */
typedef struct {
    uint8_t type; /**< Type of the frame. @see ExpansionFrameType. */
} ExpansionFrameHeader;

/**
 * @brief Heartbeat frame contents.
 */
typedef struct {
    /** Empty. */
} ExpansionFrameHeartbeat;

/**
 * @brief Status frame contents.
 */
typedef struct {
    uint8_t error; /**< Reported error code. @see ExpansionFrameError. */
} ExpansionFrameStatus;

/**
 * @brief Baud rate frame contents.
 */
typedef struct {
    uint32_t baud; /**< Requested baud rate. */
} ExpansionFrameBaudRate;

/**
 * @brief Control frame contents.
 */
typedef struct {
    uint8_t command; /**< Control command number. @see ExpansionFrameControlCommand. */
} ExpansionFrameControl;

/**
 * @brief Data frame contents.
 */
typedef struct {
    /** Size of the data. Must be less than EXPANSION_MAX_DATA_SIZE. */
    uint8_t size;
    /** Data bytes. Valid only up to ExpansionFrameData::size bytes. */
    uint8_t bytes[EXPANSION_MAX_DATA_SIZE];
} ExpansionFrameData;

/**
 * @brief Expansion protocol frame structure.
 */
typedef struct {
    ExpansionFrameHeader header; /**< Header of the frame. Required. */
    union {
        ExpansionFrameHeartbeat heartbeat; /**< Heartbeat frame contents. */
        ExpansionFrameStatus status; /**< Status frame contents. */
        ExpansionFrameBaudRate baud_rate; /**< Baud rate frame contents. */
        ExpansionFrameControl control; /**< Control frame contents. */
        ExpansionFrameData data; /**< Data frame contents. */
    } content; /**< Contents of the frame. */
} ExpansionFrame;

#pragma pack(pop)

/**
 * @brief Receive function type declaration.
 *
 * @see expansion_frame_decode().
 *
 * @param[out] data pointer to the buffer to reveive the data into.
 * @param[in] data_size maximum output buffer capacity, in bytes.
 * @param[in,out] context pointer to a user-defined context object.
 * @returns number of bytes written into the output buffer.
 */
typedef size_t (*ExpansionFrameReceiveCallback)(uint8_t* data, size_t data_size, void* context);

/**
 * @brief Send function type declaration.
 *
 * @see expansion_frame_encode().
 *
 * @param[in] data pointer to the buffer containing the data to be sent.
 * @param[in] data_size size of the data to send, in bytes.
 * @param[in,out] context pointer to a user-defined context object.
 * @returns number of bytes actually sent.
 */
typedef size_t (*ExpansionFrameSendCallback)(const uint8_t* data, size_t data_size, void* context);

/**
 * @brief Get encoded frame size.
 *
 * The frame MUST be complete and properly formed.
 *
 * @param[in] frame pointer to the frame to be evaluated.
 * @returns encoded frame size, in bytes.
 */
static size_t expansion_frame_get_encoded_size(const ExpansionFrame* frame) {
    switch(frame->header.type) {
    case ExpansionFrameTypeHeartbeat:
        return sizeof(frame->header);
    case ExpansionFrameTypeStatus:
        return sizeof(frame->header) + sizeof(frame->content.status);
    case ExpansionFrameTypeBaudRate:
        return sizeof(frame->header) + sizeof(frame->content.baud_rate);
    case ExpansionFrameTypeControl:
        return sizeof(frame->header) + sizeof(frame->content.control);
    case ExpansionFrameTypeData:
        return sizeof(frame->header) + sizeof(frame->content.data.size) + frame->content.data.size;
    default:
        return 0;
    }
}

/**
 * @brief Get remaining number of bytes needed to properly decode a frame.
 *
 * The return value will vary depending on the received_size parameter value.
 * The frame is considered complete when the function returns 0.
 *
 * @param[in] frame pointer to the frame to be evaluated.
 * @param[in] received_size number of bytes currently availabe for evaluation.
 * @returns number of bytes needed for a complete frame.
 */
static size_t
    expansion_frame_get_remaining_size(const ExpansionFrame* frame, size_t received_size) {
    if(received_size < sizeof(ExpansionFrameHeader)) return sizeof(ExpansionFrameHeader);

    const size_t received_content_size = received_size - sizeof(ExpansionFrameHeader);
    size_t content_size;

    switch(frame->header.type) {
    case ExpansionFrameTypeHeartbeat:
        content_size = 0;
        break;
    case ExpansionFrameTypeStatus:
        content_size = sizeof(frame->content.status);
        break;
    case ExpansionFrameTypeBaudRate:
        content_size = sizeof(frame->content.baud_rate);
        break;
    case ExpansionFrameTypeControl:
        content_size = sizeof(frame->content.control);
        break;
    case ExpansionFrameTypeData:
        if(received_content_size < sizeof(frame->content.data.size)) {
            content_size = sizeof(frame->content.data.size);
        } else {
            content_size = sizeof(frame->content.data.size) + frame->content.data.size;
        }
        break;
    default:
        return SIZE_MAX;
    }

    return content_size > received_content_size ? content_size - received_content_size : 0;
}

/**
 * @brief Receive and decode a frame.
 *
 * Will repeatedly call the receive callback function until enough data is gathered.
 *
 * @param[out] frame pointer to the frame to contain decoded data.
 * @param[in] receive pointer to the function used to receive data.
 * @param[in,out] context pointer to a user-defined context object. Will be passed to the receive callback function.
 * @returns true if a frame was successfully received and decoded, false otherwise.
 */
static bool expansion_frame_decode(
    ExpansionFrame* frame,
    ExpansionFrameReceiveCallback receive,
    void* context) {
    size_t total_size = 0;
    size_t remaining_size;

    while(true) {
        remaining_size = expansion_frame_get_remaining_size(frame, total_size);
        if(remaining_size == 0 || remaining_size == SIZE_MAX) break;

        const size_t received_size =
            receive((uint8_t*)frame + total_size, remaining_size, context);
        if(received_size == 0) break;

        total_size += received_size;
    }

    return remaining_size == 0;
}

/**
 * @brief Encode and send a frame.
 *
 * @param[in] frame pointer to the frame to be encoded and sent.
 * @param[in] send pointer to the function used to send data.
 * @param[in,out] context pointer to a user-defined context object. Will be passed to the send callback function.
 * @returns true if a frame was successfully encoded and sent, false otherwise.
 */
static bool expansion_frame_encode(
    const ExpansionFrame* frame,
    ExpansionFrameSendCallback send,
    void* context) {
    const size_t encoded_size = expansion_frame_get_encoded_size(frame);

    if(encoded_size != 0) {
        return send((const uint8_t*)frame, encoded_size, context) == encoded_size;
    } else {
        return false;
    }
}

#ifdef __cplusplus
}
#endif
