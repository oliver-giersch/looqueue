{ // TODO: update cached tail pointer here?

	// re-load the tail pointer to check if it has already been advanced (FIXME:
	// better not?)
	auto curr = tag_ptr_t { this->m_tail.load(relaxed) };

	// another thread has already advanced the tail pointer, so this thread can
	// retry and will likely enter the fast-path
	// FIXME: This is not in the original M&S algorithm, may be worse for
	// contention
	if (tail != curr.decompose_ptr()) {
		tail->increment_enqueue_count();
		return detail::advance_tail_res_t::ADVANCED;
	}

	// load the current tail's next pointer to check if another thread has already
	// appended a new node to the queue but has not yet updated the tail pointer
	auto next = tail->next.load(relaxed);
	if (next == nullptr) {
		// there is no new node yet, allocate a new one and attempt to append it
		auto node = new node_t(elem);
		auto advanced = detail::advance_tail_res_t::ADVANCED;
		const auto res
			= tail->next.compare_exchange_strong(next, node, release, relaxed);
		if (res) {
			// the CAS succeeded in appending the node after the tail, now the tail
			// has to be updated
			if (bounded_cas_loop(this->m_tail, curr, tag_ptr_t { node, 1 }, tail,
						release)) {
				final_count = curr.decompose_tag() - NODE_SIZE;
			}

			// it doesn't matter, which thread succeeded in updating the tail, since
			// all must attempt to set it to previous tail's next pointer, which was
			// set by this thread and contains `elem`
			next = node;
			advanced = detail::advance_tail_res_t::ADVANCED_AND_INSERTED;
		} else {
			if (bounded_cas_loop(this->m_tail, curr, tag_ptr_t { next, 1 }, tail,
						release)) {
				final_count = curr.decompose_tag() - NODE_SIZE;
			}
		}

		// update the cached tail pointer
		auto expected = tail;
		this->m_cached_tail.compare_exchange_strong(expected, next, release,
			relaxed);
		// conclude the operation by increasing the enqueue count to allow
		// reclamation
		tail->increment_enqueue_count(final_count);

		if (!res) {
			// the CAS failed so another thread must have succeeded in appending a
			// node, delete the node allocated by this thread and try again
			delete node;
		}

		return advanced;
	} else {
		// there is already a new node after the current tail, so this thread has to
		// help updating the queue's tail pointer and retry once the tail has been
		// advanced
		if (bounded_cas_loop(this->m_tail, curr, tag_ptr_t { next, 1 }, tail,
					release)) {
			final_count = curr.decompose_tag() - NODE_SIZE;
		}

		// update the cached tail pointer
		auto expected = tail;
		this->m_cached_tail.compare_exchange_strong(expected, next, release,
			relaxed);
		// conclude the operation by increasing the enqueue count to allow
		// reclamation
		tail->increment_enqueue_count(final_count);

		return detail::advance_tail_res_t::ADVANCED;
	}