#ifndef BIRD_APKASSETVFS_H
#define BIRD_APKASSETVFS_H

#include "../VFS.h"

struct AAssetManager;

namespace bird::platform {

/**
 * Reads assets packaged inside the APK. The Gradle build stages the
 * engine's baked assets under the APK's assets/ root keeping their repo
 * paths, so "assets/shaders/ui.vert.bird" resolves unchanged.
 */
class ApkAssetVFS final : public VFS {
   public:
	/** @param manager The activity's asset manager; must outlive this. */
	explicit ApkAssetVFS(AAssetManager* manager);

	std::optional<std::vector<uint8_t>> readFile(
		const std::string& path) const override;
	bool exists(const std::string& path) const override;

   private:
	AAssetManager* m_manager;
};

}  // namespace bird::platform

#endif	// BIRD_APKASSETVFS_H
