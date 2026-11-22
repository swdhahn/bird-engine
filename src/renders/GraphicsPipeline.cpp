//
// Created by Seth Hahn on 11/6/22.
//

#include "GraphicsPipeline.h"

#include <cstring>
#include <memory>

#include "../animation/skeletal/PoseOps.h"
#include "../animation/skeletal/SkeletalAnimationComponent.h"
#include "generic/Material.h"
#include "generic/Mesh.h"
#include "generic/RawBuffer.h"

namespace bird {

GraphicsPipelineType CURRENT_GRAPHICS_PIPELINE = GRAPHICS_PIPELINE_NONE;

std::shared_ptr<Mesh> GraphicsPipeline::s_quad_mesh = nullptr;

uint32_t WINDOW_WIDTH = 1280;
uint32_t WINDOW_HEIGHT = 720;

GraphicsPipeline::GraphicsPipeline(GraphicsPipelineType graphicsPipeline) {
	CURRENT_GRAPHICS_PIPELINE = graphicsPipeline;
	m_defaultCamera = new DefaultCamera(
		WINDOW_WIDTH, WINDOW_HEIGHT);  // TODO: Make this configurable
	m_camera = m_defaultCamera;
	m_orthoMatrix = glm::ortho(0.0f, (float)WINDOW_WIDTH, (float)WINDOW_HEIGHT,
							   0.0f, -1.0f, 1.0f);
}

GraphicsPipeline::~GraphicsPipeline() {
	delete m_defaultCamera;
}

void GraphicsPipeline::initializeGraphicSpecifics() {
	std::unique_ptr<GlobalUBOData[]> globalUBOData =
		std::make_unique<GlobalUBOData[]>(1);
	m_globalUBO = BufferBuilder<GlobalUBOData>(BUFFER_UNIFORM | BUFFER_STAGED)
					  .data(std::move(globalUBOData), 1)
					  .bindingPoint(UBO_BINDING_GLOBAL)
					  .build();

	const float vertices[] = {
		-1.0f, 1.0f,  0.0f,	 // 0: Top-Left
		-1.0f, -1.0f, 0.0f,	 // 1: Bottom-Left
		1.0f,  1.0f,  0.0f,	 // 2: Top-Right
		1.0f,  -1.0f, 0.0f	 // 3: Bottom-Right
	};

	const float texCoords[] = {
		0.0f, 1.0f,	 // 0: Top-Left
		0.0f, 0.0f,	 // 1: Bottom-Left
		1.0f, 1.0f,	 // 2: Top-Right
		1.0f, 0.0f	 // 3: Bottom-Right
	};

	const unsigned int indices[] = {
		0, 1, 2,  // Triangle 1
		2, 1, 3	  // Triangle 2
	};

	std::unique_ptr<float[]> vert =
		std::make_unique<float[]>(sizeof(float) * 3 * 4);
	memcpy(vert.get(), vertices, sizeof(float) * 3 * 4);
	std::unique_ptr<float[]> tex =
		std::make_unique<float[]>(sizeof(float) * 2 * 4);
	memcpy(tex.get(), texCoords, sizeof(float) * 2 * 4);
	std::unique_ptr<uint32_t[]> ind =
		std::make_unique<uint32_t[]>(sizeof(uint32_t) * 6);
	memcpy(ind.get(), indices, sizeof(uint32_t) * 6);

	std::unique_ptr<bird::Buffer<float>> vertexBuffer =
		BufferBuilder<float>(bird::BUFFER_STAGED | bird::BUFFER_ARRAY)
			.data(std::move(vert), 3 * 4)
			.build();
	std::unique_ptr<bird::Buffer<float, 2>> texCoordBuffer =
		BufferBuilder<float, 2>(bird::BUFFER_STAGED | bird::BUFFER_ARRAY)
			.data(std::move(tex), 2 * 4)
			.build();
	std::unique_ptr<bird::Buffer<uint32_t>> indicesBuffer =
		BufferBuilder<uint32_t>(bird::BUFFER_STAGED |
								bird::BUFFER_ELEMENT_ARRAY)
			.data(std::move(ind), 6)
			.build();
	s_quad_mesh = MeshBuilder()
					  .setIndexBuffer(std::move(indicesBuffer))
					  .setVertexBuffer(std::move(vertexBuffer))
					  .setTextureCoordBuffer(std::move(texCoordBuffer))
					  .build();
}

std::unique_ptr<Window>& GraphicsPipeline::getWindow() {
	return m_pWindow;
}

void GraphicsPipeline::setCamera(Camera* camera) {
	m_camera = camera;
	// A camera built before a resize must adopt the current aspect ratio.
	m_camera->setViewportSize(WINDOW_WIDTH, WINDOW_HEIGHT);
}

void GraphicsPipeline::resize(uint32_t framebufferWidth,
							  uint32_t framebufferHeight, uint32_t uiWidth,
							  uint32_t uiHeight) {
	WINDOW_WIDTH = framebufferWidth;
	WINDOW_HEIGHT = framebufferHeight;
	m_orthoMatrix = glm::ortho(0.0f, static_cast<float>(uiWidth),
							   static_cast<float>(uiHeight), 0.0f, -1.0f, 1.0f);
	m_defaultCamera->setViewportSize(framebufferWidth, framebufferHeight);
	m_camera->setViewportSize(framebufferWidth, framebufferHeight);
}

Camera* GraphicsPipeline::getCamera() {
	return m_camera;
}

void GraphicsPipeline::addMesh(MeshComponent* mesh) {
	m_meshes.emplace_back(mesh);
}

void GraphicsPipeline::removeMesh(MeshComponent* mesh) {
	m_meshes.erase(std::remove(m_meshes.begin(), m_meshes.end(), mesh),
				   m_meshes.end());
}

void GraphicsPipeline::addLight(LightComponent* light) {
	if (std::find(m_lights.begin(), m_lights.end(), light) == m_lights.end()) {
		m_lights.push_back(light);
	}
}

void GraphicsPipeline::removeLight(LightComponent* light) {
	m_lights.erase(std::remove(m_lights.begin(), m_lights.end(), light),
				   m_lights.end());
}

GlobalUBOData GraphicsPipeline::buildGlobalData() const {
	const WorldEnvironment& env = m_camera->getWorldEnvironment();
	GlobalUBOData global{};
	global.perspective = m_camera->getPerspectiveMatrix();
	global.view = m_camera->getViewMatrix();
	global.viewPos = Vector4(m_camera->getWorldPosition(), 1.0f);
	global.inverseProjection = glm::inverse(global.perspective);
	global.inverseView = glm::inverse(global.view);
	const float width = static_cast<float>(WINDOW_WIDTH);
	const float height = static_cast<float>(WINDOW_HEIGHT);
	global.screenSize = Vector4(width, height, 1.0f / width, 1.0f / height);
	global.ambientAndExposure =
		Vector4(env.ambientColor * env.ambientIntensity, env.exposure);
	global.cameraParams =
		Vector4(m_camera->getNearPlane(), m_camera->getFarPlane(),
				static_cast<float>(env.debugView), 0.0f);
	return global;
}

namespace {

/** The object's skeletal animation, or null. O(components). */
SkeletalAnimationComponent* findSkeletalAnimation(WorldObject* object) {
	for (auto& component : object->getComponents()) {
		if (auto* skeletal =
				dynamic_cast<SkeletalAnimationComponent*>(component.get())) {
			return skeletal;
		}
	}
	return nullptr;
}

}  // namespace

void GraphicsPipeline::buildRenderQueue(const Frustum& frustum) {
	m_renderQueue.clear();
	m_shadowCasters.clear();
	m_frustumCuller.beginFrame(frustum);
	const Matrix4 view = m_camera->getViewMatrix();
	for (MeshComponent* component : m_meshes) {
		WorldObject* object = component->world_object();
		const Matrix4& model = object->getTransformMatrix();
		// Skinned meshes without an animation component draw in bind pose.
		SkeletalAnimationComponent* skeletal = findSkeletalAnimation(object);
		const int meshCount = component->getMeshCount();
		for (int i = 0; i < meshCount; i++) {
			Material* material = component->getMaterial(i).get();
			if (material == nullptr) continue;
			RenderItem item;
			item.mesh = component->getMeshes()[i].get();
			item.material = material;
			item.model = model;
			AABB localBounds = item.mesh->getLocalBounds();
			if (skeletal && item.mesh->isSkinned()) {
				item.skinPalette = &skeletal->getSkinningPalette();
				const AABB posed =
					skinnedBounds(skeletal->getAnimator().getModelMatrices(),
								  item.mesh->getJointRadii());
				if (!posed.isEmpty()) localBounds = posed;
			}
			item.worldBounds = localBounds.transformed(model);
			if (material->castsShadows()) m_shadowCasters.push_back(item);
			const RenderBucket bucket = classifyMaterial(*material);
			if (!m_frustumCuller.testMesh(item.worldBounds, bucket)) continue;
			item.viewDepth =
				-(view * Vector4(item.worldBounds.center(), 1.0f)).z;
			m_renderQueue.submit(item);
		}
	}
	m_renderQueue.sort();
}

std::vector<LightSnapshot> GraphicsPipeline::snapshotLights() {
	std::vector<LightSnapshot> snapshots;
	snapshots.reserve(m_lights.size());
	for (LightComponent* light : m_lights) {
		snapshots.push_back(snapshotLight(*light));
	}
	return snapshots;
}
}  // namespace bird
