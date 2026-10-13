#include "PathProbe.h"

#include <xbyak/xbyak.h>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

namespace PathProbe
{
	namespace
	{
		constexpr std::ptrdiff_t kUpdateThisOffset = 0x18;
		constexpr std::size_t    kMaxSeen = 8192;

		std::mutex                         g_lock;
		std::unordered_set<std::uintptr_t> g_seen;
		struct Link
		{
			std::uintptr_t base;
			std::ptrdiff_t off;
			std::uintptr_t key;
		};
		std::unordered_set<std::uintptr_t>   g_owner;
		std::unordered_map<RE::FormID, Link> g_follower;
		std::unordered_set<std::uintptr_t>   g_unmatched;

		void __cdecl OnUpdate(std::uintptr_t a_this)
		{
			std::scoped_lock l(g_lock);
			if (g_seen.size() < kMaxSeen) {
				g_seen.insert(a_this - kUpdateThisOffset);
			}
		}

		// Path follower update: note which follower objects are alive, then run the original.
		struct Thunk : Xbyak::CodeGenerator
		{
			Thunk(std::uintptr_t a_original)
			{
				push(rcx);
				push(rdx);
				push(r8);
				push(r9);
				sub(rsp, 0x68);
				movdqu(ptr[rsp + 0x20], xmm0);
				movdqu(ptr[rsp + 0x30], xmm1);
				movdqu(ptr[rsp + 0x40], xmm2);
				movdqu(ptr[rsp + 0x50], xmm3);
				mov(rax, reinterpret_cast<std::uintptr_t>(&OnUpdate));
				call(rax);
				movdqu(xmm0, ptr[rsp + 0x20]);
				movdqu(xmm1, ptr[rsp + 0x30]);
				movdqu(xmm2, ptr[rsp + 0x40]);
				movdqu(xmm3, ptr[rsp + 0x50]);
				add(rsp, 0x68);
				pop(r9);
				pop(r8);
				pop(rdx);
				pop(rcx);
				mov(rax, a_original);
				jmp(rax);
			}
		};

		template <class T>
		bool SafeRead(std::uintptr_t a_addr, T& a_out)
		{
			__try {
				a_out = *reinterpret_cast<const T*>(a_addr);
				return true;
			} __except (EXCEPTION_EXECUTE_HANDLER) {
				return false;
			}
		}
	}

	void Install()
	{
		REL::Relocation<std::uintptr_t> vtbl{ RE::VTABLE_MovementAgentPathFollowerStandard[1] };
		const auto   original = *reinterpret_cast<std::uintptr_t*>(vtbl.address() + 2 * sizeof(void*));
		static Thunk thunk{ original };
		thunk.ready();
		DWORD old = 0;
		VirtualProtect(const_cast<std::uint8_t*>(thunk.getCode()), thunk.getSize(), PAGE_EXECUTE_READ, &old);
		vtbl.write_vfunc(2, reinterpret_cast<std::uintptr_t>(thunk.getCode()));
		logger::info("PathProbe: hooked path follower update (original at image+{:#x})", original - REL::Module::get().base());
	}

	void Refresh(const std::vector<RE::Actor*>& a_actors)
	{
		std::unordered_set<std::uintptr_t> seen;
		{
			std::scoped_lock l(g_lock);
			seen.swap(g_seen);
		}

		std::unordered_map<std::uintptr_t, RE::FormID> keys;
		for (auto* a : a_actors) {
			const auto id = a->GetFormID();
			keys[reinterpret_cast<std::uintptr_t>(a)] = id;
			keys[reinterpret_cast<std::uintptr_t>(a->AsActorState())] = id;
			if (auto* mc = a->GetActorRuntimeData().movementController.get()) {
				keys[reinterpret_cast<std::uintptr_t>(mc)] = id;
			}
		}

		static int calls = 0;
		if (++calls % 20 == 0) {
			g_unmatched.clear();
		}
		if (g_owner.size() > 4096) {
			g_owner.clear();
			g_follower.clear();
		}
		for (auto base : seen) {
			if (g_owner.contains(base) || g_unmatched.contains(base)) {
				continue;
			}
			bool found = false;
			for (std::ptrdiff_t off = 0; off < 0x200 && !found; off += 8) {
				std::uintptr_t v = 0;
				if (SafeRead(base + off, v)) {
					if (auto it = keys.find(v); it != keys.end()) {
						g_owner.insert(base);
						g_follower[it->second] = Link{ base, off, v };
						found = true;
						static bool logged = false;
						if (!logged) {
							logged = true;
							logger::info("PathProbe: follower->actor link found at follower+{:#x}", off);
						}
					}
				}
			}
			if (!found && g_unmatched.size() < 4096) {
				g_unmatched.insert(base);
			}
		}
	}

	bool ReadPath(std::uintptr_t a_path, PathGeometry& a_out)
	{
		if (!a_path) {
			return false;
		}
		__try {
			const auto data = *reinterpret_cast<const std::uintptr_t*>(a_path + 0x90);
			const auto total = *reinterpret_cast<const std::uint32_t*>(a_path + 0xA0);
			if (!data || total == 0 || total > 4096) {
				return false;
			}
			a_out.total = total;
			a_out.count = (std::min)(total, PathGeometry::kMaxNodes);
			a_out.incomplete = *reinterpret_cast<const bool*>(a_path + 0xA8);
			for (std::uint32_t i = 0; i < a_out.count; ++i) {
				const auto node = data + i * 0x48;
				auto&      n = a_out.nodes[i];
				n.pos = *reinterpret_cast<const RE::NiPoint3*>(node);
				n.action = *reinterpret_cast<const std::uintptr_t*>(node + 0x30) != 0;
				n.tangent = *reinterpret_cast<const RE::NiPoint3*>(node + 0x38);
			}
			return true;
		} __except (EXCEPTION_EXECUTE_HANDLER) {
			return false;
		}
	}

	std::optional<PathRef> GetPathRef(RE::Actor* a_actor)
	{
		auto it = g_follower.find(a_actor->GetFormID());
		if (it == g_follower.end()) {
			return std::nullopt;
		}
		const auto [base, off, key] = it->second;
		std::uintptr_t check = 0;
		PathRef        r;
		if (!SafeRead(base + off, check) || check != key) {
			g_owner.erase(base);
			g_follower.erase(it);
			return std::nullopt;
		}
		if (!SafeRead(base + 0x128, r.path) || !SafeRead(base + 0x100, r.progress) || !SafeRead(base + 0x118, r.state)) {
			return std::nullopt;
		}
		return r;
	}
}
