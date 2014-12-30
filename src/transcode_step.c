int transcode_step() {
	OutputStream *ost;
	InputStream  *ist;
	int ret;

	ost = choose_output(); // ****<<  选择适合的输出流(该轮到音频,视频还是字幕输出了??)
	if (!ost) {
		if (got_eagain()) {
			reset_eagain();
			av_usleep(10000);
			return 0;
		}
		av_log(NULL, AV_LOG_VERBOSE, "No more inputs to read from, finishing.\n");
		return AVERROR_EOF;
	}

	if (ost->filter) {
		if ((ret = transcode_from_filter(ost->filter->graph, &ist)) < 0)
			return ret;
		if (!ist)
			return 0;
	} else {
		av_assert0(ost->source_index >= 0);
		ist = input_streams[ost->source_index]; // ****<<  选择输入流(即是指定的输出流)
	}

	ret = process_input(ist->file_index); // ****<<  see process_input.c
	if (ret == AVERROR(EAGAIN)) {
		if (input_files[ist->file_index]->eagain)
			ost->unavailable = 1;
		return 0;
	}
	if (ret < 0)
		return ret == AVERROR_EOF ? 0 : ret;

	return reap_filters(); // ****<<  see reap_filters.c
}