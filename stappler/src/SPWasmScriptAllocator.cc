/*
 * Copyright 2018 Roman Katuntsev <sbkarr@stappler.org>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "SPWasmScriptRuntime.h"

NS_SP_EXT_BEGIN(wasm)

struct MemCtx {
	uint8_t *data = nullptr;
	uint32_t size = 0;

	::wasm::Thread *thread = nullptr;
	const RuntimeMemory *mem = nullptr;
	ScriptRuntime::Allocator *alloc = nullptr;
	std::mutex *mutex = nullptr;

	MemCtx(::wasm::Thread *thread)
	: MemCtx(thread, thread->GetMemoryPtr(0)) {
		if (auto threadPtr = (ScriptThread *)thread->getThreadContext()) {
			mutex = &(static_cast<ScriptRuntime *>(threadPtr->getRuntime())->getMutex());
		}
	}

	MemCtx(::wasm::Thread *thread, const RuntimeMemory *mem)
	: data(mem->data), size(mem->size), thread(thread), mem(mem) {
		alloc = ((ScriptRuntime::MemoryContext *)mem->ctx)->allocator;
	}

	MemCtx(::wasm::Thread *thread, const RuntimeMemory *mem, std::mutex &mutex)
	: data(mem->data), size(mem->size), thread(thread), mem(mem), mutex(&mutex) {
		alloc = ((ScriptRuntime::MemoryContext *)mem->ctx)->allocator;
	}

	void lock() { if (mutex) { mutex->lock(); } }
	void unlock() { if (mutex) { mutex->unlock(); } }
};

// Align on a power of 2 boundary
static constexpr uint32_t ALIGN(uint32_t size, uint32_t boundary) { return (((size) + ((boundary) - 1)) & ~((boundary) - 1)); }

// Default alignment
static constexpr uint32_t ALIGN_DEFAULT(uint32_t size) { return ALIGN(size, 16); }

static constexpr uint32_t BOUNDARY_INDEX ( 12 );
static constexpr uint32_t BOUNDARY_SIZE ( 1 << BOUNDARY_INDEX );

static constexpr uint32_t MIN_ALLOC (2 * BOUNDARY_SIZE);
static constexpr uint32_t MAX_INDEX ( 20 );

static constexpr uint32_t BlockThreshold = 256;

using memptr_t = uint32_t;

template <typename Type>
struct MemPtr {
	static constexpr auto null_value = std::numeric_limits<uint32_t>::max();

	//static MemPtr<Type> address(uint8_t *mem, Type *ptr) { return MemPtr<Type>{memptr_t((uint8_t *)ptr - mem)}; }
	static MemPtr<Type> address(MemCtx &mem, Type *ptr) { return MemPtr<Type>{memptr_t((uint8_t *)ptr - mem.mem->data)}; }

	MemPtr() = default;
	MemPtr(nullptr_t) : ptr(null_value) { }
	MemPtr(memptr_t ptr) : ptr(ptr) { }
	MemPtr(const MemPtr &ptr) = default;

	MemPtr &operator=(const MemPtr &) = default;
	MemPtr &operator=(nullptr_t) { ptr = null_value; return *this;  }

	//operator bool() const { return ptr != null_value; }

	//Type * get(uint8_t *mem) const { return (ptr != null_value) ? (Type *) &mem[ptr] : nullptr; }
	Type * get(MemCtx &mem) const { return (ptr != null_value) ? (Type *) &(mem.mem->data)[ptr] : nullptr; }

	memptr_t addr() const { return (ptr != null_value) ? ptr : 0; }

	bool operator == (nullptr_t) const { return ptr == null_value; }
	bool operator != (nullptr_t) const { return ptr != null_value; }

	bool operator == (const MemPtr &other) const { return ptr == other.ptr; }
	bool operator != (const MemPtr &other) const { return ptr != other.ptr; }

	MemPtr &operator += (uint32_t offset) {
		ptr += offset * sizeof(Type);
		return *this;
	}

	template <typename T>
	MemPtr<T> reinterpret() {
		return MemPtr<T>(ptr);
	}

	memptr_t ptr = null_value;
};

struct MemNode {
	MemPtr<MemNode> next; // next memnode
	MemPtr<MemPtr<MemNode>> ref; // reference to self
	uint32_t index; // size
	uint32_t free_index; // how much free
	MemPtr<uint8_t> first_avail; // pointer to first free memory
	MemPtr<uint8_t> endp; // pointer to end of free memory

	void insert(MemCtx &mem, MemPtr<MemNode> point);
	void remove(MemCtx &mem);
	uint32_t free_space() const;
};

struct Cleanup {
	using callback_t = int (*)(void *data);

	MemPtr<Cleanup> next;
	const void *data;
	callback_t fn;

	static void run(MemCtx &mem, MemPtr<Cleanup> *cref) {
		Cleanup *c = cref->get(mem);
		while (c) {
			*cref = c->next;
			(*c->fn)((void *)c->data);
			c = cref->get(mem);
		}
	}
};

struct ScriptRuntime::Allocator {
	uint32_t last = 0; // largest used index into free
	uint32_t origin = 0;
	MemPtr<MemNode> buf[MAX_INDEX];

	Allocator(uint32_t origin);
	~Allocator();

	MemPtr<MemNode> alloc(MemCtx &mem, uint32_t in_size);
	void free(MemCtx &mem, MemPtr<MemNode>);
};

using Allocator = ScriptRuntime::Allocator;

struct MemAddr {
	uint32_t size = 0;
	MemPtr<MemAddr> next = nullptr;
	MemPtr<uint8_t> address = nullptr;
};

struct Pool {
	MemPtr<Pool> parent = nullptr;
	MemPtr<Pool> child = nullptr;
	MemPtr<Pool> sibling = nullptr;
	MemPtr<MemPtr<Pool>> ref = nullptr;
	MemPtr<Cleanup> cleanups = nullptr;
	MemPtr<Cleanup> free_cleanups = nullptr;
	MemPtr<Cleanup> pre_cleanups = nullptr;

	MemPtr<MemNode> active = nullptr;
	MemPtr<MemNode> self = nullptr; /* The node containing the pool itself */
	MemPtr<uint8_t> self_first_avail = nullptr;

	MemPtr<MemAddr> buffered = nullptr;
	MemPtr<MemAddr> free_buffered = nullptr;

	static Pool *create(MemCtx &mem, uint32_t initAlloc = MIN_ALLOC);
	static void destroy(MemCtx &mem, Pool *);

	Pool(MemPtr<MemNode> node);

	void do_destroy(MemCtx &mem);

	MemPtr<uint8_t> alloc_buf(MemCtx &mem, size_t &sizeInBytes);
	void free_buf(MemCtx &mem, MemPtr<uint8_t> ptr, size_t sizeInBytes);

	MemPtr<uint8_t> alloc(MemCtx &mem, size_t &sizeInBytes);
	void free(MemCtx &mem, MemPtr<uint8_t> ptr, size_t sizeInBytes);

	MemPtr<uint8_t> palloc(MemCtx &mem, uint32_t);

	void clear(MemCtx &mem);

	Pool *make_child(MemCtx &mem);

	/*void cleanup_register(void *, cleanup_t::callback_t cb);
	void pre_cleanup_register(void *, cleanup_t::callback_t cb);

	void cleanup_kill(void *, cleanup_t::callback_t cb);
	void cleanup_run(void *, cleanup_t::callback_t cb);*/
};

struct ThreadContext {
	struct PoolCtx {
		MemPtr<Pool> pool;
		MemPtr<PoolCtx> next;
	};

	MemPtr<Pool> root;
	MemPtr<PoolCtx> poolStack;
	MemPtr<PoolCtx> unused;

	ThreadContext(MemCtx &mem, MemPtr<Pool> pool);
	void push(MemCtx &mem, MemPtr<Pool> pool);
	void push(MemCtx &mem, Pool *pool);
	void pop(MemCtx &mem);
	MemPtr<Pool> getRoot() const;
	MemPtr<Pool> top(MemCtx &mem) const;
};

template <typename T>
auto ADDRESS(MemCtx &mem, T *ptr) -> MemPtr<T> {
	return MemPtr<T>::address(mem, ptr);
}

template <typename Type>
inline memptr_t operator-(const MemPtr<Type> &l, const MemPtr<Type> &r) {
	return l.ptr - r.ptr;
}

template <typename Type>
inline auto operator+(const MemPtr<Type> &l, const uint32_t &r) -> MemPtr<Type> {
	return MemPtr<Type>(l.ptr + r);
}

constexpr size_t SIZEOF_MEMNODE_T ( ALIGN_DEFAULT(sizeof(MemNode)) );
constexpr size_t SIZEOF_POOL_T ( ALIGN_DEFAULT(sizeof(Pool)) );

void MemNode::insert(MemCtx &mem, MemPtr<MemNode> point) {
	this->ref = point.get(mem)->ref;
	*this->ref.get(mem) = ADDRESS(mem, this);
	this->next = point;
	point.get(mem)->ref = ADDRESS(mem, &this->next);
}

void MemNode::remove(MemCtx &mem) {
	*this->ref.get(mem) = this->next;
	this->next.get(mem)->ref = this->ref;
}

uint32_t MemNode::free_space() const {
	return endp - first_avail;
}

Allocator::Allocator(uint32_t origin) : origin(ALIGN_DEFAULT(origin) + ALIGN_DEFAULT(sizeof(Allocator))) {
	// WebAssembly memory already cleared
}

Allocator::~Allocator() { }

MemPtr<MemNode> Allocator::alloc(MemCtx &mem, uint32_t in_size) {
	std::unique_lock<MemCtx> lock;

	uint32_t size = ALIGN(in_size + SIZEOF_MEMNODE_T, BOUNDARY_SIZE);
	if (size < in_size) {
		return nullptr;
	}

	if (size < MIN_ALLOC) {
		size = MIN_ALLOC;
	}

	size_t index = (size >> BOUNDARY_INDEX) - 1;
	if (index > maxOf<uint32_t>()) {
		return nullptr;
	}

	/* First see if there are any nodes in the area we know our node will fit into. */
	if (index <= last) {
		lock = std::unique_lock<MemCtx>(mem);

		/* Walk the free list to see if there are any nodes on it of the requested size */
		uint32_t max_index = last;
		auto ref = &buf[index];
		uint32_t i = index;
		while (*ref == nullptr && i < max_index) {
			ref++;
			i++;
		}

		MemNode *node = nullptr;
		if ((node = ref->get(mem)) != nullptr) {
			/* If we have found a node and it doesn't have any nodes waiting in line behind it _and_ we are on
			 * the highest available index, find the new highest available index
			 */
			if ((*ref = node->next) == nullptr && i >= max_index) {
				do {
					ref--;
					max_index--;
				}
				while (*ref == nullptr && max_index > 0);
				last = max_index;
			}

			node->next = nullptr;
			node->first_avail = ADDRESS(mem, (uint8_t *)node + SIZEOF_MEMNODE_T);
			return ADDRESS(mem, node);
		}
	} else if (buf[0] != nullptr) {
		lock = std::unique_lock<MemCtx>(mem);

		/* If we found nothing, seek the sink (at index 0), if it is not empty. */

		/* Walk the free list to see if there are any nodes on it of the requested size */
		MemNode *node = nullptr;
		auto ref = &buf[0];
		while ((node = ref->get(mem)) != nullptr && index > node->index) {
			ref = &node->next;
		}

		if (node) {
			*ref = node->next;
			node->next = nullptr;
			node->first_avail = ADDRESS(mem, (uint8_t *)node + SIZEOF_MEMNODE_T);
			return ADDRESS(mem, node);
		}
	}

	if (lock.owns_lock()) {
		lock.unlock();
	}

	/* If we haven't got a suitable node, malloc a new one
	 * and initialize it.
	 */

	if (origin + size > mem.size) {
		if (!mem.thread->GrowMemory(mem.mem, size / ::wasm::WABT_PAGE_SIZE + 1)) {
			return nullptr;
		} else {
			mem.data = mem.mem->data;
			mem.size = mem.mem->size;
		}
	}

	MemNode *node = nullptr;
	if ((node = new (mem.data + origin) MemNode()) == nullptr) {
		return nullptr;
	}

	origin += size;

	node->next = nullptr;
	node->index = (uint32_t)index;
	node->first_avail = ADDRESS(mem, (uint8_t *)node + SIZEOF_MEMNODE_T);
	node->endp = ADDRESS(mem, (uint8_t *)node + size);

	return ADDRESS(mem, node);
}

void Allocator::free(MemCtx &mem, MemPtr<MemNode> nodePtr) {
	MemPtr<MemNode> next = nullptr;
	MemNode *node = nodePtr.get(mem);

	std::unique_lock<MemCtx> lock(mem);
	uint32_t max_index = last;

	/* Walk the list of submitted nodes and free them one by one, shoving them in the right 'size' buckets as we go. */
	do {
		next = node->next;
		uint32_t index = node->index;

		if (index < MAX_INDEX) {
			/* Add the node to the appropiate 'size' bucket.  Adjust the max_index when appropiate. */
			if ((node->next = buf[index]) == nullptr && index > max_index) {
				max_index = index;
			}
			buf[index] = ADDRESS(mem, node);
		} else {
			/* This node is too large to keep in a specific size bucket, just add it to the sink (at index 0). */
			node->next = buf[0];
			buf[0] = ADDRESS(mem, node);
		}
	} while ((node = next.get(mem)) != nullptr);

	if (lock.owns_lock()) {
		lock.unlock();
	}

	last = max_index;
}


MemPtr<uint8_t> Pool::alloc_buf(MemCtx &mem, size_t &sizeInBytes) {
	if (buffered != nullptr) {
		MemPtr<MemAddr> addr;
		MemPtr<MemAddr> *lastp;

		addr = buffered;
		lastp = &buffered;
		while (addr != nullptr) {
			auto c = addr.get(mem);
			if (c->size > sizeInBytes * 2) {
				break;
			} else if (c->size >= sizeInBytes) {
				*lastp = c->next;
				c->next = free_buffered;
				free_buffered = addr;
				sizeInBytes = c->size;
				return c->address;
			}

			lastp = &c->next;
			addr = c->next;
		}
	}
	return palloc(mem, sizeInBytes);
}

void Pool::free_buf(MemCtx &mem, MemPtr<uint8_t> ptr, size_t sizeInBytes) {
	MemPtr<MemAddr> addrPtr;

	if (free_buffered != nullptr) {
		addrPtr = free_buffered;
		free_buffered = addrPtr.get(mem)->next;
	} else {
		addrPtr = palloc(mem, sizeof(MemAddr)).reinterpret<MemAddr>();
	}

	auto addr = addrPtr.get(mem);
	addr->size = sizeInBytes;
	addr->address = ptr;
	addr->next = nullptr;

	if (buffered != nullptr) {
		MemPtr<MemAddr> cPtr;
		MemPtr<MemAddr> *lastp;

		cPtr = buffered;
		lastp = &buffered;
		while (cPtr != nullptr) {
			auto c = cPtr.get(mem);
			if (c->size >= sizeInBytes) {
				addr->next = cPtr;
				*lastp = cPtr;
				break;
			}

			lastp = &c->next;
			cPtr = c->next;
		}

		if (addr->next == nullptr) {
			*lastp = cPtr;
		}
	} else {
		buffered = addrPtr;
		addr->next = nullptr;
	}
}

MemPtr<uint8_t> Pool::alloc(MemCtx &mem, size_t &sizeInBytes) {
	if (sizeInBytes >= BlockThreshold) {
		return alloc_buf(mem, sizeInBytes);
	}
	return palloc(mem, sizeInBytes);
}

void Pool::free(MemCtx &mem, MemPtr<uint8_t> ptr, size_t sizeInBytes) {
	if (sizeInBytes >= BlockThreshold) {
		free_buf(mem, ptr, sizeInBytes);
	}
}

MemPtr<uint8_t> Pool::palloc(MemCtx &ctx, uint32_t in_size) {
	MemNode *active;
	MemPtr<uint8_t> mem;
	uint32_t size, free_index;

	size = ALIGN_DEFAULT(in_size);
	if (size < in_size) {
		return nullptr;
	}
	active = this->active.get(ctx);

	/* If the active node has enough bytes left, use it. */
	if (size <= active->free_space()) {
		mem = active->first_avail;
		active->first_avail += size;
		return mem;
	}

	MemNode *node = active->next.get(ctx);
	if (size <= node->free_space()) {
		node->remove(ctx);
	} else {
		if ((node = ctx.alloc->alloc(ctx, size).get(ctx)) == nullptr) {
			return nullptr;
		}
	}

	node->free_index = 0;
	mem = node->first_avail;
	node->first_avail += size;

	node->insert(ctx, ADDRESS(ctx, active));

	this->active = ADDRESS(ctx, node);

	free_index = (ALIGN(active->endp - active->first_avail + 1, BOUNDARY_SIZE) - BOUNDARY_SIZE) >> BOUNDARY_INDEX;

	active->free_index = (uint32_t)free_index;
	node = active->next.get(ctx);
	if (free_index >= node->free_index) {
		return mem;
	}

	do {
		node = node->next.get(ctx);
	} while (free_index < node->free_index);

	active->remove(ctx);
	active->insert(ctx, ADDRESS(ctx, node));

	return mem;
}

void Pool::clear(MemCtx &mem) {
	Cleanup::run(mem, &this->pre_cleanups);
	this->pre_cleanups = nullptr;

	while (this->child != nullptr) {
		this->child.get(mem)->do_destroy(mem);
	}

	/* Run cleanups */
	Cleanup::run(mem, &this->cleanups);
	this->cleanups = nullptr;
	this->free_cleanups = nullptr;


	/* Find the node attached to the pool structure, reset it, make it the active node and free the rest of the nodes. */
	auto activePtr = this->active = this->self;
	auto active = activePtr.get(mem);
	active->first_avail = this->self_first_avail;

	if (active->next == activePtr) {
		buffered = nullptr;
		free_buffered = nullptr;
		return;
	}

	*active->ref.get(mem) = nullptr;
	if (active->next != nullptr) {
		mem.alloc->free(mem, active->next);
	}
	active->next = activePtr;
	active->ref = ADDRESS(mem, &active->next);
	buffered = nullptr;
	free_buffered = nullptr;
}

Pool *Pool::create(MemCtx &mem, uint32_t initAlloc) {
	auto nodePtr = mem.alloc->alloc(mem, initAlloc - SIZEOF_MEMNODE_T);
	auto node = nodePtr.get(mem);
	node->next = ADDRESS(mem, node);
	node->ref = ADDRESS(mem, &node->next);

	Pool *pool = new (node->first_avail.get(mem)) Pool(nodePtr);
	node->first_avail = pool->self_first_avail = MemPtr<uint8_t>(ALIGN_DEFAULT((node->first_avail + SIZEOF_POOL_T).addr()));

	return pool;
}

void Pool::destroy(MemCtx &mem, Pool *pool) {
	pool->do_destroy(mem);
}

Pool::Pool(MemPtr<MemNode> node) : active(node), self(node) { }

void Pool::do_destroy(MemCtx &mem) {
	Cleanup::run(mem, &this->pre_cleanups);
	this->pre_cleanups = nullptr;

	while (this->child != nullptr) {
		this->child.get(mem)->do_destroy(mem);
	}

	Cleanup::run(mem, &this->cleanups);

	/* Remove the pool from the parents child list */
	if (this->parent != nullptr) {
		std::unique_lock<MemCtx> lock(mem);
		auto sib = this->sibling;
		*this->ref.get(mem) = this->sibling;
		if (sib != nullptr) {
			sib.get(mem)->ref = this->ref;
		}
	}

	auto active = this->self;
	*active.get(mem)->ref.get(mem) = nullptr;

	mem.alloc->free(mem, active);
}

Pool *Pool::make_child(MemCtx &mem) {
	Pool *p = this;
	MemPtr<MemNode> nodePtr;
	if ((nodePtr = mem.alloc->alloc(mem, MIN_ALLOC - SIZEOF_MEMNODE_T)) == nullptr) {
		return nullptr;
	}

	auto node = nodePtr.get(mem);

	node->next = nodePtr;
	node->ref = ADDRESS(mem, &node->next);

	Pool *pool = new (node->first_avail.get(mem)) Pool(nodePtr);
	node->first_avail = pool->self_first_avail = node->first_avail + SIZEOF_POOL_T;

	if ((pool->parent = ADDRESS(mem, p)) != nullptr) {
		std::unique_lock<MemCtx> lock(mem);
		pool->sibling = this->child;
		if (pool->sibling != nullptr) {
			pool->sibling.get(mem)->ref = ADDRESS(mem, &sibling);
		}

		child = ADDRESS(mem, pool);
		pool->ref = ADDRESS(mem, &child);
	}

	return pool;
}

ThreadContext::ThreadContext(MemCtx &mem, MemPtr<Pool> pool)
: root(pool) {
	push(mem, pool);
}

void ThreadContext::push(MemCtx &mem, MemPtr<Pool> pool) {
	if (unused != nullptr) {
		auto node = unused;
		auto stackNode = unused.get(mem);
		unused = stackNode->next; // pop from unused nodes

		stackNode->pool = pool;
		stackNode->next = poolStack;
		poolStack = node; // push to stack
	} else {
		auto rootPool = root.get(mem);
		size_t size = sizeof(PoolCtx);
		auto node = rootPool->palloc(mem, size).reinterpret<PoolCtx>();
		PoolCtx * stackNode = new (node.get(mem)) PoolCtx();
		stackNode->pool = pool;
		stackNode->next = poolStack;
		poolStack = node;
	}
}

void ThreadContext::push(MemCtx &mem, Pool *pool) {
	push(mem, ADDRESS(mem, pool));
}

void ThreadContext::pop(MemCtx &mem) {
	auto tmpStack = poolStack;
	auto stack = poolStack.get(mem);
	poolStack = stack->next; // pop from stack;

	stack->next = unused;
	unused = tmpStack; // push to unused;
}

MemPtr<Pool> ThreadContext::getRoot() const {
	return root;
}

MemPtr<Pool> ThreadContext::top(MemCtx &mem) const {
	return poolStack.get(mem)->pool;
}

NS_SP_EXT_END(wasm)
