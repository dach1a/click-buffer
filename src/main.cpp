#include <Geode/Geode.hpp>
#include <Geode/modify/PlayerObject.hpp>

using namespace geode::prelude;

class $modify(BufferPlayer, PlayerObject) {
	struct Fields {
		int pending = 0;       // pasos de física restantes del click guardado
		int releaseDelay = 0;  // pasos para soltar después de un click sintético
		bool userHolding = false;
		bool synthetic = false;
	};

	bool isMainPlayer() {
		auto pl = PlayLayer::get();
		return pl && (this == pl->m_player1 || this == pl->m_player2);
	}

	bool enabled() {
		return Mod::get()->getSettingValue<bool>("enabled");
	}

	// ¿Hay algún orbe tocándose que todavía no se haya activado?
	bool touchingFreshRing() {
		if (!m_touchingRings) return false;
		CCObject* obj;
		CCARRAY_FOREACH(m_touchingRings, obj) {
			auto ring = static_cast<GameObject*>(obj);
			if (!ring) continue;
			bool used = m_isSecondPlayer ? ring->m_hasBeenActivatedP2
			                             : ring->m_hasBeenActivatedP1;
			if (!used) return true;
		}
		return false;
	}

	// ¿Un click ahora mismo haría algo útil?
	bool canActNow() {
		if (touchingFreshRing()) return true;
		// En ship/ufo/wave/swing el suelo no requiere click especial
		if (m_isShip || m_isBird || m_isDart || m_isSwing) return false;
		return m_isOnGround;
	}

	bool pushButton(PlayerButton btn) {
		if (btn != PlayerButton::Jump || !isMainPlayer() || !enabled()) {
			return PlayerObject::pushButton(btn);
		}

		auto f = m_fields.self();
		if (f->synthetic) return PlayerObject::pushButton(btn);

		f->userHolding = true;

		// Si ya puede actuar, comportamiento normal. Si no, guardamos el click.
		if (!canActNow()) {
			f->pending = static_cast<int>(
				Mod::get()->getSettingValue<int64_t>("buffer-frames")
			);
		} else {
			f->pending = 0;
		}
		return PlayerObject::pushButton(btn);
	}

	bool releaseButton(PlayerButton btn) {
		if (btn != PlayerButton::Jump || !isMainPlayer() || !enabled()) {
			return PlayerObject::releaseButton(btn);
		}

		auto f = m_fields.self();
		if (f->synthetic) return PlayerObject::releaseButton(btn);

		f->userHolding = false;
		return PlayerObject::releaseButton(btn);
	}

	void update(float dt) {
		if (isMainPlayer() && enabled()) {
			auto f = m_fields.self();

			// Soltar el click sintético si el usuario ya no está presionando
			if (f->releaseDelay > 0 && --f->releaseDelay == 0 && !f->userHolding) {
				f->synthetic = true;
				PlayerObject::releaseButton(PlayerButton::Jump);
				f->synthetic = false;
			}

			// Ejecutar el click guardado en cuanto sea válido
			if (f->pending > 0) {
				if (canActNow()) {
					f->pending = 0;
					f->synthetic = true;
					PlayerObject::pushButton(PlayerButton::Jump);
					f->synthetic = false;
					if (!f->userHolding) f->releaseDelay = 2;
				} else {
					f->pending--;
				}
			}
		}
		PlayerObject::update(dt);
	}
};
