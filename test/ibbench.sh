#!/usr/bin/env bash

declare ARGS='--ib-dev=mlx5_0 --iters=10000'

case $(hostname) in
	'ffmk-n1')
		ARGS+=' --ib-port=1 --report-unsorted'
		;;
	'ffmk-n2')
		ARGS+=' --ib-port=2 -H 141.76.48.45'
		;;
	'ffmk-n3')
		ARGS+=' -H 141.76.48.45'
		;;
esac

exec ib_send_lat $ARGS
