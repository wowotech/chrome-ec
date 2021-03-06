/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "queue.h"
#include "queue_policies.h"
#include "uartn.h"
#include "usart.h"
#include "usb-stream.h"
#ifdef CONFIG_STREAM_SIGNATURE
#include "signing.h"
#endif

#define USE_UART_INTERRUPTS (!(defined(CONFIG_CUSTOMIZED_RO) && \
defined(SECTION_IS_RO)))
#define QUEUE_SIZE 64


#ifdef CONFIG_STREAM_USART1
struct usb_stream_config const ap_usb;
struct usart_config const ap_uart;

#ifdef CONFIG_STREAM_SIGNATURE
/*
 * This code adds the ability to capture UART data received, and
 * sign it with H1's key. This allows the log output to be verified
 * as actual UART output from this board.
 *
 * This functionality is enabled by redirecting the UART receive queue
 * to feed into the signing module rather than the usb tx. After being
 * added to the running hash, the data is then pushed by the signer
 * into the usb tx queue.
 */
struct signer_config const sig;
static struct queue const ap_uart_output =
	QUEUE_DIRECT(QUEUE_SIZE, uint8_t, ap_uart.producer, sig.consumer);
static struct queue const sig_to_usb =
	QUEUE_DIRECT(QUEUE_SIZE, uint8_t, sig.producer, ap_usb.consumer);

SIGNER_CONFIG(sig, stream_uart, sig_to_usb, ap_uart_output);

#else  /* Not CONFIG_STREAM_SIGNATURE */
static struct queue const ap_uart_output =
	QUEUE_DIRECT(QUEUE_SIZE, uint8_t, ap_uart.producer, ap_usb.consumer);
#endif

static struct queue const ap_usb_to_uart =
	QUEUE_DIRECT(QUEUE_SIZE, uint8_t, ap_usb.producer, ap_uart.consumer);

/*
 * AP UART data is sent to the ap_uart_output queue, and received from
 * the ap_usb_to_uart queue. The ap_uart_output queue is received by the
 * USB bridge, or if a signer is enabled, received by the signer, which then
 * passes the data to the USB bridge after processing it.
 */
USART_CONFIG(ap_uart,
	     UART_AP,
	     ap_uart_output,
	     ap_usb_to_uart);

/*
 * The UART USB bridge receives character data from the UART's queue,
 * unless signing is enabled, in which case it receives data from the
 * signer's queue, after the signer has received it from the UART and
 * processed it.
 */
USB_STREAM_CONFIG(ap_usb,
		  USB_IFACE_AP,
		  USB_STR_AP_NAME,
		  USB_EP_AP,
		  USB_MAX_PACKET_SIZE,
		  USB_MAX_PACKET_SIZE,
		  ap_usb_to_uart,
#ifdef CONFIG_STREAM_SIGNATURE
		  sig_to_usb)
#else
		  ap_uart_output)
#endif
#endif  /* CONFIG_STREAM_USART1 */

#ifdef CONFIG_STREAM_USART2
struct usb_stream_config const ec_usb;
struct usart_config const ec_uart;

static struct queue const ec_uart_to_usb =
	QUEUE_DIRECT(QUEUE_SIZE, uint8_t, ec_uart.producer, ec_usb.consumer);
static struct queue const ec_usb_to_uart =
	QUEUE_DIRECT(QUEUE_SIZE, uint8_t, ec_usb.producer, ec_uart.consumer);

USART_CONFIG(ec_uart,
	     UART_EC,
	     ec_uart_to_usb,
	     ec_usb_to_uart);

USB_STREAM_CONFIG(ec_usb,
		  USB_IFACE_EC,
		  USB_STR_EC_NAME,
		  USB_EP_EC,
		  USB_MAX_PACKET_SIZE,
		  USB_MAX_PACKET_SIZE,
		  ec_usb_to_uart,
		  ec_uart_to_usb)
#endif

void get_data_from_usb(struct usart_config const *config)
{
	struct queue const *uart_out = config->consumer.queue;
	int c;

	/* Copy output from buffer until TX fifo full or output buffer empty */
	while (queue_count(uart_out) && QUEUE_REMOVE_UNITS(uart_out, &c, 1))
		uartn_write_char(config->uart, c);

	/* If output buffer is empty, disable transmit interrupt */
	if (!queue_count(uart_out))
		uartn_tx_stop(config->uart);
}

void send_data_to_usb(struct usart_config const *config)
{
	struct queue const *uart_in = config->producer.queue;

	/* Copy input from buffer until RX fifo empty or the queue is full */
	while (uartn_rx_available(config->uart) && queue_space(uart_in)) {
		int c = uartn_read_char(config->uart);

		QUEUE_ADD_UNITS(uart_in, &c, 1);
	}
}

static void uart_read(struct producer const *producer, size_t count)
{
}

static void uart_written(struct consumer const *consumer, size_t count)
{
	struct usart_config const *config =
		DOWNCAST(consumer, struct usart_config, consumer);

	if (uartn_tx_ready(config->uart), queue_count(consumer->queue))
		uartn_tx_start(config->uart);
}

static void uart_flush(struct consumer const *consumer)
{
	struct usart_config const *config =
		DOWNCAST(consumer, struct usart_config, consumer);

	uartn_tx_flush(config->uart);
}

struct producer_ops const uart_producer_ops = {
	.read = uart_read,
};

struct consumer_ops const uart_consumer_ops = {
	.written = uart_written,
	.flush   = uart_flush,
};

#if USE_UART_INTERRUPTS
#ifdef CONFIG_STREAM_USART1
/*
 * Interrupt handlers for UART1
 */
CONFIGURE_INTERRUPTS(ap_uart,
		     GC_IRQNUM_UART1_RXINT,
		     GC_IRQNUM_UART1_TXINT)
#endif

#ifdef CONFIG_STREAM_USART2
/*
 * Interrupt handlers for UART2
 */
CONFIGURE_INTERRUPTS(ec_uart,
		     GC_IRQNUM_UART2_RXINT,
		     GC_IRQNUM_UART2_TXINT)
#endif
#endif
