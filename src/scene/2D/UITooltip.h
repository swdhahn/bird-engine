#ifndef BIRD_UITOOLTIP_H
#define BIRD_UITOOLTIP_H

#include <string>

#include "UIObject.h"

namespace bird {

/** Small hint box shown by UIManager near the cursor; not interactive. */
class UITooltip : public UIObject {
   public:
	explicit UITooltip(std::string text);

	const std::string& getText() const { return m_text; }
	void draw(UIDrawList& list) override;

   protected:
	Vector2 measureContent(Vector2 available) override;

   private:
	std::string m_text;
};

}  // namespace bird

#endif	// !BIRD_UITOOLTIP_H
