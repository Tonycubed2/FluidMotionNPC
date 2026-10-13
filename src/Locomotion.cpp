// FluidMotionNPC - NPC-only traversal pose layer.
// Stripped from Fluid Motion's Locomotion.cpp (GPL-3.0, megakuutti). Everything for the player (movement, camera, input,
// motion matching, heading), every always-on pose effect (lean, duck, step-over, head lead, landing crouch, stop settle,
// wall touch) and the clip playback-speed hook are removed. What is left only plays the climb / ledge-hang / jump clips
// on NPCs while JumpNav or Mantle is moving them, and starts climbs/hangs when an NPC in a hurry is blocked by a ledge.
//
// Compatibility change from the original: the graph bone redirection (graph output written into dummy nodes, then
// copied back with the climb pose on top) is only installed on an NPC while it is actually jumping or climbing, and
// removed again shortly after. Outside of a traversal the NPC skeleton is never touched.

#include "Locomotion.h"
#include "AnimMath.h"
#include "ClimbClip.h"
#include "JumpNav.h"
#include "Mantle.h"
#include "Tuning.h"

namespace Locomotion
{
	using namespace AnimMath;

	namespace
	{
		Config::Anim               g_cfg;
		std::atomic<float>         g_time{ 0.0f };
		std::atomic<std::uint32_t> g_frame{ 0 };
		std::atomic<std::uint32_t> g_preRenderFrame{ 0 };

		// Frames the bone redirection stays on after the last climb/jump pose, so short gaps between clips don't
		// hook and unhook every frame.
		constexpr std::uint32_t kRedirectHold = 30;

		constexpr const char* kBoneCom = "NPC COM [COM ]";
		constexpr const char* kSpine[3] = { "NPC Spine [Spn0]", "NPC Spine1 [Spn1]", "NPC Spine2 [Spn2]" };
		constexpr const char* kHead = "NPC Head [Head]";
		constexpr const char* kThigh[2] = { "NPC L Thigh [LThg]", "NPC R Thigh [RThg]" };
		constexpr const char* kCalf[2] = { "NPC L Calf [LClf]", "NPC R Calf [RClf]" };
		constexpr const char* kFoot[2] = { "NPC L Foot [Lft ]", "NPC R Foot [Rft ]" };
		constexpr const char* kUArm[2] = { "NPC L UpperArm [LUar]", "NPC R UpperArm [RUar]" };
		constexpr const char* kFArm[2] = { "NPC L Forearm [LLar]", "NPC R Forearm [RLar]" };
		constexpr const char* kHand[2] = { "NPC L Hand [LHnd]", "NPC R Hand [RHnd]" };

		struct Bones
		{
			RE::NiAVObject *com = nullptr, *spine[3]{}, *head = nullptr;
			RE::NiAVObject *thigh[2]{}, *calf[2]{}, *foot[2]{}, *uarm[2]{}, *farm[2]{}, *hand[2]{};
			bool            arms = false;
		};

		struct Redirect
		{
			struct Entry
			{
				RE::BShkbAnimationGraph* graph;
				int                      idx;
				RE::NiNode*              real;
				RE::NiNode*              dummy;
				RE::NiNode*              origNode;
				std::uint32_t            origSlot;
			};
			std::vector<Entry>                                        redir;
			std::vector<RE::BSTSmartPointer<RE::BShkbAnimationGraph>> graphRefs;
			std::vector<RE::NiPointer<RE::NiNode>>                    dummies;
			RE::NiPointer<RE::NiAVObject>                             root;
			RE::NiAVObject*                                           failedRoot = nullptr;
			std::mutex                                                m;
			std::vector<RE::NiTransform>                              pose;
			std::atomic<bool>                                         hooked{ false };
			std::atomic<bool>                                         busy{ false };
		};

		void Unhook(Redirect& pr, const char* a_why);

		struct Slot
		{
			~Slot();
			Redirect      pr;
			std::mutex    m;
			RE::FormID    id = 0;
			std::uint32_t sampleFrame = 0, postFrame = 0, climbFrame = 0, drivenFrame = 0, bonesFrame = 0, commitFrame = 0;
			float         sampleT = -9.0f, lastPostT = 0, airStart = -9.0f;
			bool          havePos = false, prevGrounded = false, bonesBad = false;
			V3            pos{}, vel{};
			float         speedH = 0, driven = 0, cmdSpeed = 0, cmdT = -9.0f;
			ClimbPose     climbPose;
			std::vector<RE::NiAVObject*>                              cnodes;
			std::vector<RE::NiAVObject*>                              jnodes[3];
			RE::NiPointer<RE::NiAVObject>                             root;
			Bones                                                     b;
			std::vector<std::pair<RE::NiAVObject*, RE::NiTransform>> commit;
		};

		std::mutex                                            g_tableMutex;
		std::unordered_map<RE::FormID, std::shared_ptr<Slot>> g_table;

		std::shared_ptr<Slot> Find(RE::FormID a_id)
		{
			std::lock_guard lk(g_tableMutex);
			const auto      it = g_table.find(a_id);
			return it == g_table.end() ? nullptr : it->second;
		}

		bool Active() { return g_cfg.enabled && g_cfg.npcs && Tuning::GetMode() == Tuning::Mode::kFluid; }

		bool Traversing(const Slot& s)
		{
			return s.climbFrame != 0 && g_frame.load(std::memory_order_relaxed) - s.climbFrame <= kRedirectHold;
		}

		RE::NiAVObject* FindBone(RE::NiAVObject* a_root, const char* a_name)
		{
			auto* n = a_root->GetObjectByName(RE::BSFixedString(a_name));
			if (n) {
				return n;
			}
			if (std::string_view(a_name) == kFoot[0]) {
				if (n = a_root->GetObjectByName(RE::BSFixedString("NPC L Foot [LLft ]")); n) {
					return n;
				}
			}
			std::string alt = a_name;
			if (const auto p = alt.find(" L "); p != std::string::npos) {
				if (const auto q = alt.find("[L", p); q != std::string::npos) {
					alt[q + 1] = 'R';
					if (n = a_root->GetObjectByName(RE::BSFixedString(alt.c_str())); n) {
						return n;
					}
				}
			}
			std::string base = a_name;
			if (const auto p = base.find(" ["); p != std::string::npos) {
				base.resize(p);
			}
			RE::NiAVObject*                      found = nullptr;
			std::function<void(RE::NiAVObject*)> walk = [&](RE::NiAVObject* o) {
				if (!o || found) {
					return;
				}
				std::string_view nm = o->name.c_str();
				if (nm == base || (nm.starts_with(base) && nm.substr(base.size()).starts_with(" ["))) {
					found = o;
					return;
				}
				if (auto* nd = o->AsNode()) {
					for (auto& ch : nd->GetChildren()) {
						walk(ch.get());
					}
				}
			};
			walk(a_root);
			return found;
		}

		void ResolveBones(Slot& s, RE::NiAVObject* a_root)
		{
			s.root.reset(a_root);
			s.b = Bones{};
			s.bonesFrame = g_frame.load();
			auto  get = [&](const char* a_name) { return FindBone(a_root, a_name); };
			auto& b = s.b;
			b.com = get(kBoneCom);
			b.head = get(kHead);
			bool ok = b.com && b.head;
			for (int i = 0; i < 3; ++i) {
				b.spine[i] = get(kSpine[i]);
				ok = ok && b.spine[i];
			}
			bool arms = true;
			for (int i = 0; i < 2; ++i) {
				b.thigh[i] = get(kThigh[i]);
				b.calf[i] = get(kCalf[i]);
				b.foot[i] = get(kFoot[i]);
				ok = ok && b.thigh[i] && b.calf[i] && b.foot[i];
				b.uarm[i] = get(kUArm[i]);
				b.farm[i] = get(kFArm[i]);
				b.hand[i] = get(kHand[i]);
				arms = arms && b.uarm[i] && b.farm[i] && b.hand[i];
			}
			b.arms = arms;
			if (ok) {
				ok = b.spine[0]->parent && b.spine[1]->parent == b.spine[0] && b.spine[2]->parent == b.spine[1] && b.com->parent;
				for (int i = 0; i < 2 && ok; ++i) {
					ok = b.calf[i]->parent == b.thigh[i] && b.foot[i]->parent == b.calf[i];
				}
			}
			if (b.arms) {
				for (int i = 0; i < 2; ++i) {
					b.arms = b.arms && b.farm[i]->parent == b.uarm[i] && b.hand[i]->parent == b.farm[i];
				}
			}
			s.cnodes.clear();
			for (auto& j : s.jnodes) {
				j.clear();
			}
			if (ok) {
				if (const auto* clip = ClimbClip::Get()) {
					for (const auto& n : clip->bones) {
						s.cnodes.push_back(get(n.c_str()));
					}
				}
			}
			s.bonesBad = !ok;
			if (!ok && g_cfg.log) {
				logger::info("ANIM actor {:08X}: skeleton not recognised, traversal poses off for it", s.id);
			}
		}

		bool SafeFormID(RE::Actor* a_actor, RE::FormID& a_id)
		{
			if (!a_actor || reinterpret_cast<std::uintptr_t>(a_actor) < 0x10000) {
				return false;
			}
			__try {
				a_id = a_actor->GetFormID();
				return a_id != 0;
			} __except (EXCEPTION_EXECUTE_HANDLER) {
				return false;
			}
		}

		// Graph speed of an actor moved by hand (route legs, jumps, climbs) follows the speed it is moved at, so it
		// runs instead of standing still while gliding. Also records the commanded speed for the climb trigger.
		struct SpeedChannel
		{
			static void thunk(void* a_this, bool a_arg)
			{
				func(a_this, a_arg);
				if (!Active()) {
					return;
				}
				auto*      actor = *reinterpret_cast<RE::Actor**>(static_cast<char*>(a_this) + 0x28);
				RE::FormID id = 0;
				if (!SafeFormID(actor, id) || id == 0x14) {
					return;
				}
				auto slot = Find(id);
				if (!slot) {
					return;
				}
				auto&           s = *slot;
				float&          v = *reinterpret_cast<float*>(static_cast<char*>(a_this) + 0x18);
				const auto      frame = g_frame.load();
				std::lock_guard lk(s.m);
				s.cmdSpeed = v;
				s.cmdT = g_time.load();
				if (frame - s.drivenFrame <= 2) {
					v = s.driven;
				}
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		void ReachArm(const Bones& B, int i, const V3& a_target, const V3& a_pole, float w)
		{
			RE::NiAVObject* clav = B.uarm[i]->parent;
			const V3        sh = B.uarm[i]->world.translate;
			if (clav && clav->parent && w > 0.01f) {
				const float armLen = (B.farm[i]->world.translate - sh).Length() + (B.hand[i]->world.translate - B.farm[i]->world.translate).Length();
				const V3    to = a_target - sh;
				const float d = to.Length(), shift = Clamp((d - 0.7f * armLen) * 0.6f, 0.0f, 8.0f) * w;
				const V3    cp = clav->world.translate;
				if (shift > 0.2f && d > 1e-3f) {
					RotateWorld(clav, RotBetween(Norm(sh - cp, a_pole), Norm(sh + to * (shift / d) - cp, a_pole)));
					Refresh(clav);
				}
			}
			const V3 cur = B.hand[i]->world.translate;
			SolveTwoBone(B.uarm[i], B.farm[i], B.hand[i], cur * (1.0f - w) + a_target * w, a_pole);
		}

		void PostClimb(RE::Actor* a_actor, Slot& s)
		{
			RE::NiAVObject* root = a_actor->Get3D(false);
			const auto*     clip = ClimbClip::Get(s.climbPose.clip, s.climbPose.rig);
			if (!root || !clip || s.bonesBad) {
				return;
			}
			const auto frame = g_frame.load();
			if (s.root.get() != root || frame - s.bonesFrame > 600) {
				ResolveBones(s, root);
				if (s.bonesBad) {
					return;
				}
			}
			const Bones& B = s.b;
			const auto&  cp = s.climbPose;
			const bool   jumpClip = cp.clip >= ClimbClip::kJumpStart;
			auto&        nodes = jumpClip ? s.jnodes[cp.clip - ClimbClip::kJumpStart] : s.cnodes;
			if (jumpClip && nodes.empty()) {
				for (const auto& name : clip->bones) {
					nodes.push_back(FindBone(root, name.c_str()));
				}
			}
			if (nodes.size() != clip->bones.size()) {
				return;
			}
			for (std::size_t k = 0; k < nodes.size(); ++k) {
				auto* n = nodes[k];
				if (!n) {
					continue;
				}
				float cq[4], gq[4];
				clip->Rot(cp.t, k, cq);
				ToQuat(n->local.rotate, gq);
				const float sg = (gq[0] * cq[0] + gq[1] * cq[1] + gq[2] * cq[2] + gq[3] * cq[3]) < 0.0f ? -1.0f : 1.0f;
				float       r[4], len = 0;
				for (int c = 0; c < 4; ++c) {
					r[c] = gq[c] + (cq[c] * sg - gq[c]) * cp.w;
					len += r[c] * r[c];
				}
				len = 1.0f / std::sqrt(len);
				for (float& v : r) {
					v *= len;
				}
				n->local.rotate = FromQuat(r);
			}
			const auto fr = clip->Sample(cp.t);
			if (jumpClip) {
				B.com->local.translate.z += (fr.comZ - 71.5f) * cp.w;
			} else {
				B.com->local.translate.z += (fr.comZ - B.com->local.translate.z) * cp.w;
			}
			Refresh(B.com);
			if (B.arms && cp.contact > 0.01f) {
				const float ang = a_actor->data.angle.z;
				const V3    right{ std::cos(ang), -std::sin(ang), 0.0f };
				for (int i = 0; i < 2; ++i) {
					ReachArm(B, i, cp.hand[i], right * (i == 0 ? -1.0f : 1.0f), cp.contact);
				}
			}
		}

		void PoseNodes(const Slot& s, std::vector<RE::NiAVObject*>& a_out)
		{
			const Bones& B = s.b;
			a_out.clear();
			for (auto* n : { B.com, B.spine[0], B.spine[1], B.spine[2], B.head, B.thigh[0], B.thigh[1], B.calf[0], B.calf[1], B.foot[0], B.foot[1], B.uarm[0],
					 B.uarm[1], B.farm[0], B.farm[1], B.hand[0], B.hand[1] }) {
				if (n) {
					a_out.push_back(n);
				}
			}
		}

		bool SameLocal(const RE::NiTransform& a, const RE::NiTransform& b)
		{
			return std::memcmp(&a.rotate, &b.rotate, sizeof(a.rotate)) == 0 && std::memcmp(&a.translate, &b.translate, sizeof(a.translate)) == 0;
		}

		bool Recommit(Slot& s)
		{
			bool diff = false;
			for (auto& [n, t] : s.commit) {
				if (!SameLocal(n->local, t)) {
					n->local = t;
					diff = true;
				}
			}
			if (diff && s.root) {
				Refresh(s.root.get());
			}
			return diff;
		}

		void Post(RE::Actor* a_actor, Slot& s, float dt)
		{
			std::lock_guard lk(s.m);
			const auto      frame = g_frame.load();
			const float     now = g_time.load();
			if (s.postFrame == frame) {
				if (s.commitFrame == frame) {
					Recommit(s);
				}
				return;
			}
			s.postFrame = frame;
			if (!(dt > 0.0f)) {
				dt = now - s.lastPostT;
			}
			s.lastPostT = now;
			if (!(dt > 1e-4f && dt < 0.25f)) {
				return;
			}
			if (!(frame - s.climbFrame <= 2 && s.climbPose.w > 0.001f)) {
				return;
			}
			static thread_local std::vector<RE::NiAVObject*> nodes;
			static thread_local std::vector<RE::NiTransform> pre;
			auto* const                                      root = a_actor->Get3D(false);
			const bool                                       snap = root && s.root.get() == root && !s.bonesBad && s.b.com;
			if (snap) {
				PoseNodes(s, nodes);
				pre.clear();
				for (auto* n : nodes) {
					pre.push_back(n->local);
				}
			}
			PostClimb(a_actor, s);
			s.commit.clear();
			if (snap && s.root.get() == root) {
				for (std::size_t i = 0; i < nodes.size(); ++i) {
					if (!SameLocal(nodes[i]->local, pre[i])) {
						for (auto* n : nodes) {
							s.commit.emplace_back(n, n->local);
						}
						s.commitFrame = frame;
						break;
					}
				}
			}
		}

		void Unhook(Redirect& pr, const char* a_why)
		{
			if (!pr.hooked) {
				return;
			}
			std::lock_guard lk(pr.m);
			for (const auto& r : pr.redir) {
				auto& arr = r.graph->boneNodes;
				if (static_cast<std::uint32_t>(r.idx) < arr.size() && arr[r.idx].node == r.dummy) {
					r.real->local = r.dummy->local;
					arr[r.idx].node = r.origNode;
					arr[r.idx].unk08 = r.origSlot;
				}
			}
			if (pr.root) {
				Refresh(pr.root.get());
			}
			pr.redir.clear();
			pr.graphRefs.clear();
			pr.dummies.clear();
			pr.root.reset();
			pr.hooked = false;
			if (g_cfg.log) {
				static std::atomic_int logs{ 0 };
				if (logs.fetch_add(1) < 50) {
					logger::info("ANIM bone redirect removed ({})", a_why);
				}
			}
		}

		bool Below(const RE::NiAVObject* a_n, const RE::NiAVObject* a_root)
		{
			for (; a_n; a_n = a_n->parent) {
				if (a_n == a_root) {
					return true;
				}
			}
			return false;
		}

		void Hook(Redirect& pr, RE::Actor* a, RE::NiAVObject* a_root)
		{
			RE::BSTSmartPointer<RE::BSAnimationGraphManager> mgr;
			if (!a->GetAnimationGraphManager(mgr) || !mgr) {
				return;
			}
			std::vector<Redirect::Entry>                 found;
			std::unordered_map<RE::NiNode*, RE::NiNode*> made;
			for (std::uint32_t gi = 0; gi < mgr->graphs.size(); ++gi) {
				auto* g = mgr->graphs[gi].get();
				if (!g || g->boneNodes.size() == 0 || g->boneNodes.size() > 1024 || !g->boneNodes.data()) {
					continue;
				}
				auto& arr = g->boneNodes;
				for (std::uint32_t j = 0; j < arr.size(); ++j) {
					RE::NiNode* real = arr[j].node;
					const auto  slot = static_cast<std::int32_t>(arr[j].unk08);
					if (real && slot >= 0) {
						auto* tree = netimmerse_cast<RE::BSFlattenedBoneTree*>(real);
						if (!tree) {
							continue;
						}
						auto& td = tree->GetRuntimeData();
						if (static_cast<std::uint32_t>(slot) >= td.numBones || !td.boneEntries) {
							continue;
						}
						auto* n = td.boneEntries[slot].node;
						real = n ? n->AsNode() : nullptr;
					}
					if (real && Below(real, a_root) && std::string_view(real->name.c_str()).starts_with("NPC ")) {
						found.push_back({ g, static_cast<int>(j), real, nullptr, arr[j].node, arr[j].unk08 });
					}
				}
			}
			if (found.empty()) {
				pr.failedRoot = a_root;
				return;
			}
			std::vector<RE::NiPointer<RE::NiNode>> dummies;
			for (auto& r : found) {
				auto& d = made[r.real];
				if (!d) {
					RE::NiPointer<RE::NiNode> n(RE::NiNode::Create(0));
					if (!n) {
						pr.failedRoot = a_root;
						return;
					}
					n->local = r.real->local;
					dummies.push_back(n);
					d = n.get();
				}
				r.dummy = d;
			}
			for (std::uint32_t gi = 0; gi < mgr->graphs.size(); ++gi) {
				if (mgr->graphs[gi]) {
					pr.graphRefs.push_back(mgr->graphs[gi]);
				}
			}
			for (auto& r : found) {
				r.graph->boneNodes[r.idx].node = r.dummy;
				r.graph->boneNodes[r.idx].unk08 = 0xFFFFFFFFu;
			}
			std::lock_guard lk(pr.m);
			pr.redir = std::move(found);
			pr.dummies = std::move(dummies);
			pr.root.reset(a_root);
			pr.pose.clear();
			for (const auto& r : pr.redir) {
				pr.pose.push_back(r.real->local);
			}
			pr.hooked = true;
		}

		// Called right after the actor's animation graph ran. Returns true when the graph output went into the dummy
		// nodes and the pose is applied later in PosePass.
		bool AfterGraph(RE::Actor* a, Slot& s)
		{
			auto&      pr = s.pr;
			auto*      root = a->Get3D(false);
			const bool want = Active() && Traversing(s) && root && !a->IsDead() && !a->IsInRagdollState() &&
			                  g_frame.load() - g_preRenderFrame.load() <= 3;
			if (pr.hooked) {
				const char* why = !want ? "traversal over" : root != pr.root.get() ? "3D changed" : nullptr;
				if (!why) {
					for (const auto& r : pr.redir) {
						if (static_cast<std::uint32_t>(r.idx) >= r.graph->boneNodes.size() || r.graph->boneNodes[r.idx].node != r.dummy) {
							why = "bone table rebuilt";
							break;
						}
					}
				}
				if (why) {
					Unhook(pr, why);
					return false;
				}
				std::lock_guard lk(pr.m);
				for (std::size_t i = 0; i < pr.redir.size(); ++i) {
					pr.pose[i] = pr.redir[i].dummy->local;
				}
				return true;
			}
			if (want && root != pr.failedRoot) {
				Hook(pr, a, root);
			}
			return pr.hooked;
		}

		Slot::~Slot()
		{
			if (pr.hooked) {
				Unhook(pr, "slot dropped");
			}
		}

		using JobFn = void (*)(std::uint32_t);
		JobFn g_jobOrig = nullptr;

		// NPC animation graph job (runs on worker threads).
		void JobThunk(std::uint32_t a_handle)
		{
			RE::NiPointer<RE::Actor> hold;
			RE::Actor*               a = nullptr;
			std::shared_ptr<Slot>    slot;
			{
				RE::ActorHandle h;
				std::memcpy(&h, &a_handle, sizeof(a_handle));
				hold = h.get();
				a = hold.get();
				if (a && !a->IsPlayerRef()) {
					slot = Find(a->GetFormID());
				}
			}
			if (slot) {
				std::lock_guard lk(slot->pr.m);
				slot->pr.busy = true;
			}
			g_jobOrig(a_handle);
			const bool deferred = slot && AfterGraph(a, *slot);
			if (slot) {
				slot->pr.busy = false;
			}
			if (slot && Active() && !deferred) {
				Post(a, *slot, 0.0f);
			}
		}

		bool InstallJobHook()
		{
			const auto  fn = REL::ID{ 41370 }.address();
			const auto  site = fn + 0x13E;
			const auto  orig = REL::ID{ 41453 }.address();
			const auto* p = reinterpret_cast<const std::uint8_t*>(site);
			if (p[0] != 0x48 || p[1] != 0x8D || p[2] != 0x15 ||
				site + 7 + *reinterpret_cast<const std::int32_t*>(p + 3) != orig) {
				logger::error("Locomotion: NPC animation job site not as expected (different game version or another mod) - jobs not hooked");
				return false;
			}
			auto* stub = static_cast<std::uint8_t*>(SKSE::GetTrampoline().allocate(16));
			stub[0] = 0xFF;
			stub[1] = 0x25;
			*reinterpret_cast<std::int32_t*>(stub + 2) = 0;
			*reinterpret_cast<std::uintptr_t*>(stub + 6) = reinterpret_cast<std::uintptr_t>(&JobThunk);
			const auto delta = reinterpret_cast<std::intptr_t>(stub) - static_cast<std::intptr_t>(site + 7);
			if (delta > INT32_MAX || delta < INT32_MIN) {
				logger::error("Locomotion: job stub out of range");
				return false;
			}
			g_jobOrig = reinterpret_cast<JobFn>(orig);
			REL::safe_write(site + 3, static_cast<std::int32_t>(delta));
			logger::info("Locomotion: hooked the NPC animation job (image+{:#x})", orig - REL::Module::get().base());
			return true;
		}

		// NPC animation update on the main thread (Character only; the player's vtable is left alone).
		struct UpdateAnim
		{
			static void thunk(RE::Actor* a_this, float a_dt)
			{
				std::shared_ptr<Slot> slot = Find(a_this->GetFormID());
				func(a_this, a_dt);
				if (slot && !AfterGraph(a_this, *slot) && Active()) {
					Post(a_this, *slot, a_dt);
				}
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		void SampleActor(RE::Actor* a_actor, Slot& s, float now)
		{
			const auto& c = g_cfg;
			const V3    pos = a_actor->GetPosition();
			auto*       cc = a_actor->GetCharController();
			const bool  grounded = cc && cc->context.currentState == RE::hkpCharacterStateType::kOnGround;
			bool        tryClimb = false, tryHang = false;
			float       climbReach = 75.0f;
			{
				std::lock_guard lk(s.m);
				const float     dt = now - s.sampleT;
				if (s.havePos && dt > 0.004f && dt < 0.25f) {
					V3 raw = (pos - s.pos) * (1.0f / dt);
					if (raw.Length() > 3000.0f) {
						s.vel = V3{ 0, 0, 0 };
					} else {
						const float k = 1.0f - std::exp(-dt / 0.06f);
						s.vel = s.vel + (raw - s.vel) * k;
					}
					s.speedH = std::hypot(s.vel.x, s.vel.y);
				} else if (dt >= 0.25f) {
					s.vel = V3{ 0, 0, 0 };
					s.speedH = 0.0f;
				}
				if (!grounded && s.prevGrounded) {
					s.airStart = now;
				}
				s.prevGrounded = grounded;
				s.pos = pos;
				s.havePos = true;
				s.sampleT = now;
				s.sampleFrame = g_frame.load();

				const auto* st = a_actor->AsActorState();
				const bool  normal = !a_actor->IsDead() && !a_actor->IsInRagdollState() && !a_actor->IsInKillMove() && !a_actor->IsOnMount() && st &&
				                    !st->IsFlying() && !st->IsSwimming() && !st->IsBleedingOut() && st->GetSitSleepState() == RE::SIT_SLEEP_STATE::kNormal;
				const bool  busy = JumpNav::InFlight(s.id) || Mantle::Active(s.id);
				const bool  moving = s.cmdSpeed > 60.0f && now - s.cmdT < 0.2f && g_frame.load() - s.drivenFrame > 2;
				tryClimb = c.climb && normal && grounded && !busy && moving && JumpNav::InHurry(a_actor);
				climbReach = s.speedH < 40.0f ? 75.0f : 48.0f;
				tryHang = c.climb && c.hang && normal && !grounded && cc && !JumpNav::InFlight(s.id) && now - s.airStart > 0.05f &&
				          now - s.airStart < 1.8f && s.vel.z > -250.0f && moving;
			}
			if (tryClimb) {
				Mantle::TryStart(a_actor, false, nullptr, climbReach);
			} else if (tryHang) {
				Mantle::TryStart(a_actor, true, nullptr, 50.0f);
			}
		}

		// Main thread, once per frame: applies the climb/jump pose on top of the redirected graph output.
		void PosePass()
		{
			static std::vector<std::shared_ptr<Slot>> list;
			g_preRenderFrame.store(g_frame.load());
			list.clear();
			{
				std::lock_guard lk(g_tableMutex);
				for (auto& [id, s] : g_table) {
					if (s->pr.hooked) {
						list.push_back(s);
					}
				}
			}
			for (auto& s : list) {
				auto& pr = s->pr;
				auto* a = RE::TESForm::LookupByID<RE::Actor>(s->id);
				if (!a) {
					continue;
				}
				std::lock_guard lk(pr.m);
				if (!pr.hooked || pr.root.get() != a->Get3D(false) || pr.busy.load()) {
					continue;
				}
				for (std::size_t i = 0; i < pr.redir.size(); ++i) {
					pr.redir[i].real->local = pr.pose[i];
				}
				Refresh(pr.root.get());
				Post(a, *s, 0.0f);
				RE::NiUpdateData u{};
				pr.root->Update(u);
			}
		}
	}

	void Install()
	{
		g_cfg = Config::Get().anim;
		REL::Relocation<std::uintptr_t> chr{ RE::VTABLE_Character[0] };
		UpdateAnim::func = chr.write_vfunc(0x7D, UpdateAnim::thunk);
		REL::Relocation<std::uintptr_t> spd{ RE::VTABLE_BSTAnimationGraphDataChannel_Actor_float_ActorSpeedChannel_[0] };
		SpeedChannel::func = spd.write_vfunc(1, SpeedChannel::thunk);
		InstallJobHook();
		logger::info("Locomotion: NPC traversal pose layer installed (Character::UpdateAnimation, ActorSpeedChannel, NPC animation job)");
	}

	void Reload()
	{
		g_cfg = Config::Get().anim;
	}

	Config::Anim& Settings() { return g_cfg; }

	void SetClimb(RE::FormID a_id, const ClimbPose* a_pose)
	{
		auto slot = Find(a_id);
		if (!slot && a_pose && Active()) {
			// A traversal can start on an actor the per-frame scan has not reached yet.
			slot = std::make_shared<Slot>();
			slot->id = a_id;
			slot->sampleFrame = g_frame.load();
			std::lock_guard lk(g_tableMutex);
			g_table.try_emplace(a_id, slot);
			slot = g_table[a_id];
		}
		if (slot) {
			std::lock_guard lk(slot->m);
			if (a_pose) {
				slot->climbPose = *a_pose;
				slot->climbFrame = g_frame.load();
			} else {
				slot->climbPose = ClimbPose{};
			}
		}
	}

	bool IsDriven(RE::FormID a_id)
	{
		if (auto slot = Find(a_id)) {
			std::lock_guard lk(slot->m);
			return g_frame.load() - slot->drivenFrame <= 2;
		}
		return false;
	}

	void NoteDriven(RE::FormID a_id, float a_speed)
	{
		if (auto slot = Find(a_id)) {
			std::lock_guard lk(slot->m);
			slot->driven = a_speed;
			slot->drivenFrame = g_frame.load();
		}
	}

	void Reset()
	{
		{
			std::lock_guard lk(g_tableMutex);
			g_table.clear();
		}
		Mantle::Reset();
	}

	void OnFrame(float a_delta)
	{
		if (a_delta > 0.0f && a_delta <= 0.5f) {
			Mantle::OnFrame(a_delta);
		}
		if (!Active() || a_delta <= 0.0f || a_delta > 0.5f) {
			return;
		}
		const float now = g_time.load() + a_delta;
		g_time.store(now);
		const auto frame = g_frame.fetch_add(1) + 1;

		auto* player = RE::PlayerCharacter::GetSingleton();
		auto* lists = RE::ProcessLists::GetSingleton();
		if (!player || !lists || !player->Get3D(false)) {
			return;
		}

		struct Cand
		{
			float           d2;
			RE::ActorHandle h;
		};
		static std::vector<Cand> cands;
		cands.clear();
		const V3    pp = player->GetPosition();
		const float r2 = g_cfg.radius * g_cfg.radius;
		for (auto& h : lists->highActorHandles) {
			auto  ptr = h.get();
			auto* a = ptr.get();
			if (!a || a == player || a->IsDead() || !a->Is3DLoaded()) {
				continue;
			}
			const V3    d = a->GetPosition() - pp;
			const float d2 = d.x * d.x + d.y * d.y + d.z * d.z;
			if (d2 < r2) {
				cands.push_back({ d2, h });
			}
		}
		std::sort(cands.begin(), cands.end(), [](const Cand& x, const Cand& y) { return x.d2 < y.d2; });
		if (cands.size() > static_cast<std::size_t>(g_cfg.maxActors)) {
			cands.resize(g_cfg.maxActors);
		}
		for (auto& cd : cands) {
			auto  ptr = cd.h.get();
			auto* a = ptr.get();
			if (!a) {
				continue;
			}
			std::shared_ptr<Slot> slot = Find(a->GetFormID());
			if (!slot) {
				slot = std::make_shared<Slot>();
				slot->id = a->GetFormID();
				std::lock_guard lk(g_tableMutex);
				g_table[slot->id] = slot;
			}
			SampleActor(a, *slot, now);
		}
		PosePass();

		if ((frame & 63) == 0) {
			std::lock_guard lk(g_tableMutex);
			for (auto it = g_table.begin(); it != g_table.end();) {
				const auto& s = *it->second;
				if (frame - s.sampleFrame > 180 && frame - s.climbFrame > 180 && !s.pr.busy.load()) {
					it = g_table.erase(it);
				} else {
					++it;
				}
			}
		}
	}
}
