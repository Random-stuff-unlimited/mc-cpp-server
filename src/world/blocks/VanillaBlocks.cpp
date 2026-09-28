#include "world/blocks/VanillaBlocks.hpp"

#include "data/GameData.hpp"
#include "world/Level.hpp"
#include "world/blocks/Attached.hpp"
#include "world/blocks/BlockContext.hpp"
#include "world/blocks/Growth.hpp"
#include "world/blocks/Containers.hpp"
#include "world/blocks/Dispensers.hpp"
#include "world/blocks/LiquidBlock.hpp"
#include "world/blocks/Pistons.hpp"
#include "world/blocks/ProcessingBlocks.hpp"
#include "world/blocks/Redstone.hpp"
#include "world/blocks/StorageBlocks.hpp"
#include "world/blocks/Vegetation.hpp"
#include "world/feature/Trees.hpp"

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {
	bool has(const std::string& name, const char* part) { return name.find(part) != std::string::npos; }

	// BlockSetType: the sounds of a door, trapdoor, button or plate, by the material in its name ("minecraft:block.wooden")
	std::string setSound(const std::string& name, const char* kind) {
		std::string material = "wooden_";
		if (has(name, "cherry")) {
			material = "cherry_wood_";
		} else if (has(name, "crimson") || has(name, "warped")) {
			material = "nether_wood_";
		} else if (has(name, "bamboo")) {
			material = "bamboo_wood_";
		}
		return "minecraft:block." + material + kind;
	}
} // namespace

namespace {
	using Factory = std::function<std::unique_ptr<BlockBehavior>(int block)>;

	template <typename T, typename... Args> Factory make(std::shared_ptr<const BlockContext> context, Args... args) {
		return [context, args...](int) { return std::make_unique<T>(context, args...); };
	}
} // namespace

void registerVanillaBlocks(Level& level, const GameData& gameData) {
	auto context = std::make_shared<const BlockContext>(gameData);
	auto ids	 = std::make_shared<const RedstoneIds>(*context);
	auto name	 = [&gameData](int block) { return gameData.getStaticName("minecraft:block", block); };
	auto pistons = std::make_shared<const PistonIds>(*context);
	registerMovingPistons(level, context, pistons);
	auto trees	 = std::make_shared<const TreeFeatures>(gameData.getDirectory() / "tree_features.json", *context);
	using Soil	 = VegetationBlock::Soil;

	// Vanilla class -> behavior. A block takes the first class it is an instance of: subclasses come first
	std::vector<std::pair<std::string, Factory>> classes = {
			{"LiquidBlock", [](int) { return std::make_unique<LiquidBlock>(); }},

			// Plants
			{"AttachedStemBlock",
			 [context, &gameData](int block) {
				 bool pumpkin = gameData.getStaticName("minecraft:block", block) == "minecraft:attached_pumpkin_stem";
				 return std::make_unique<AttachedStemBlock>(context, context->block(pumpkin ? "minecraft:pumpkin" : "minecraft:melon"),
															context->block(pumpkin ? "minecraft:pumpkin_stem" : "minecraft:melon_stem"));
			 }},
			{"StemBlock",
			 [context, &gameData](int block) {
				 bool pumpkin = gameData.getStaticName("minecraft:block", block) == "minecraft:pumpkin_stem";
				 return std::make_unique<StemBlock>(context, context->block(pumpkin ? "minecraft:pumpkin" : "minecraft:melon"),
													context->block(pumpkin ? "minecraft:attached_pumpkin_stem" : "minecraft:attached_melon_stem"));
			 }},
			{"BeetrootBlock", [context](int block) { return std::make_unique<CropBlock>(context, block, CropBlock::Kind::Beetroot); }},
			{"TorchflowerCropBlock", [context](int block) { return std::make_unique<CropBlock>(context, block, CropBlock::Kind::Torchflower); }},
			{"CropBlock", [context](int block) { return std::make_unique<CropBlock>(context, block, CropBlock::Kind::Crop); }},
			{"NetherWartBlock", make<NetherWartBlock>(context)},
			{"SweetBerryBushBlock", make<SweetBerryBushBlock>(context)},
			{"MushroomBlock", make<MushroomBlock>(context)},
			{"FungusBlock", make<VegetationBlock>(context, Soil::Fungus)},
			{"RootsBlock", make<VegetationBlock>(context, Soil::Nether)},
			{"NetherSproutsBlock", make<VegetationBlock>(context, Soil::Nether)},
			{"WitherRoseBlock", make<VegetationBlock>(context, Soil::WitherRose)},
			{"AzaleaBlock", make<VegetationBlock>(context, Soil::DirtOrClay)},
			{"MangrovePropaguleBlock",
			 [context, trees, &gameData](int block) {
				 return std::make_unique<MangrovePropaguleBlock>(context, SaplingTreeGrower::forSapling(gameData.getStaticName("minecraft:block", block), trees, context));
			 }},
			{"SaplingBlock",
			 [context, trees, &gameData](int block) {
				 return std::make_unique<SaplingBlock>(context, Soil::Dirt, SaplingTreeGrower::forSapling(gameData.getStaticName("minecraft:block", block), trees, context));
			 }},
			{"DryVegetationBlock", make<VegetationBlock>(context, Soil::DryVegetation)},
			{"CactusFlowerBlock", make<VegetationBlock>(context, Soil::CactusFlower)},
			{"SeagrassBlock", make<SeagrassBlock>(context)},
			{"WaterlilyBlock", make<VegetationBlock>(context, Soil::Waterlily)},
			{"SeaPickleBlock", make<SeaPickleBlock>(context)},
			{"LeafLitterBlock", make<VegetationBlock>(context, Soil::SturdyTop)},
			{"TallSeagrassBlock", make<DoublePlantBlock>(context, Soil::Seagrass, true)},
			{"PitcherCropBlock", nullptr},	 // Not ported yet
			{"SmallDripleafBlock", nullptr}, // Not ported yet
			{"DoublePlantBlock", make<DoublePlantBlock>(context, Soil::Dirt, false)},
			{"VegetationBlock", make<VegetationBlock>(context, Soil::Dirt)},

			// Held by a neighbor
			{"RedstoneWallTorchBlock", [context, ids](int) { return std::make_unique<RedstoneTorchBlock>(context, ids, true); }},
			{"RedstoneTorchBlock", [context, ids](int) { return std::make_unique<RedstoneTorchBlock>(context, ids, false); }},
			{"WallTorchBlock", make<WallTorchBlock>(context)},
			{"BaseTorchBlock", make<TorchBlock>(context)},
			{"LanternBlock", make<LanternBlock>(context)},
			{"LadderBlock", make<LadderBlock>(context)},
			{"CarpetBlock", make<CarpetBlock>(context)},
			{"SnowLayerBlock", make<SnowLayerBlock>(context)},
			{"SugarCaneBlock", make<SugarCaneBlock>(context)},
			{"CactusBlock", make<CactusBlock>(context)},
			{"DoorBlock",
			 [context, ids, name](int block) {
				 std::string n	   = name(block);
				 std::string sound = has(n, "iron") ? "minecraft:block.iron_door" : has(n, "copper") ? "minecraft:block.copper_door" : setSound(n, "door");
				 return std::make_unique<PoweredDoorBlock>(context, ids, !has(n, "iron"), sound);
			 }},
			{"TrapDoorBlock",
			 [context, ids, name](int block) {
				 std::string n	   = name(block);
				 std::string sound = has(n, "iron") ? "minecraft:block.iron_trapdoor" : has(n, "copper") ? "minecraft:block.copper_trapdoor" : setSound(n, "trapdoor");
				 return std::make_unique<TrapDoorBlock>(context, ids, !has(n, "iron"), sound);
			 }},
			{"FenceGateBlock",
			 [context, ids, name](int block) {
				 std::string n = name(block);
				 std::string sound = setSound(n, "fence_gate");
				 if (sound == "minecraft:block.wooden_fence_gate") sound = "minecraft:block.fence_gate";
				 return std::make_unique<FenceGateBlock>(context, ids, sound);
			 }},
			{"BedBlock", make<BedBlock>(context)},
			{"CocoaBlock", make<CocoaBlock>(context)},

			// Redstone
			{"RedStoneWireBlock", [context, ids](int) { return std::make_unique<RedStoneWireBlock>(context, ids); }},
			{"RepeaterBlock", [context, ids](int) { return std::make_unique<RepeaterBlock>(context, ids); }},
			{"ComparatorBlock", [context, ids](int) { return std::make_unique<ComparatorBlock>(context, ids); }},
			{"LeverBlock", [context, ids](int) { return std::make_unique<LeverBlock>(context, ids); }},
			{"ButtonBlock",
			 [context, ids, name](int block) {
				 std::string n	   = name(block);
				 bool		 stone = has(n, "stone_button") || has(n, "blackstone");
				 return std::make_unique<ButtonBlock>(context, ids, stone ? 20 : 30, stone ? "minecraft:block.stone_button" : setSound(n, "button"));
			 }},
			{"WeightedPressurePlateBlock",
			 [context, ids, name](int block) {
				 bool light = has(name(block), "light_weighted");
				 return std::make_unique<PressurePlateBlock>(context, ids, PressurePlateBlock::Kind::Weighted, light ? 15 : 150,
															 "minecraft:block.metal_pressure_plate");
			 }},
			{"PressurePlateBlock",
			 [context, ids, name](int block) {
				 std::string n	   = name(block);
				 bool		 stone = has(n, "stone_pressure_plate") || has(n, "blackstone");
				 return std::make_unique<PressurePlateBlock>(context, ids, stone ? PressurePlateBlock::Kind::Mobs : PressurePlateBlock::Kind::Everything, 0,
															 stone ? "minecraft:block.stone_pressure_plate" : setSound(n, "pressure_plate"));
			 }},
			{"PistonBaseBlock",
			 [context, ids, pistons, name](int block) {
				 return std::make_unique<PistonBaseBlock>(context, ids, pistons, name(block) == "minecraft:sticky_piston");
			 }},
			{"PistonHeadBlock", [context, ids, pistons](int) { return std::make_unique<PistonHeadBlock>(context, ids, pistons); }},
			{"MovingPistonBlock", [context, ids](int) { return std::make_unique<MovingPistonBlock>(context, ids); }},
			{"DropperBlock", [context, ids](int) { return std::make_unique<DispenserBlock>(context, ids, true); }},
			// Containers
			{"TrappedChestBlock", [context, ids](int) { return std::make_unique<ChestBlock>(context, ids, ChestBlock::Kind::Trapped); }},
			{"CopperChestBlock", [context, ids](int) { return std::make_unique<ChestBlock>(context, ids, ChestBlock::Kind::Copper); }},
			{"ChestBlock", [context, ids](int) { return std::make_unique<ChestBlock>(context, ids, ChestBlock::Kind::Chest); }},
			{"BarrelBlock", [context, ids](int) { return std::make_unique<BarrelBlock>(context, ids); }},
			{"FletchingTableBlock", nullptr}, // A CraftingTableBlock that opens nothing
			{"CraftingTableBlock", [context, ids](int) { return std::make_unique<CraftingTableBlock>(context, ids); }},
			{"ShulkerBoxBlock", [context, ids](int) { return std::make_unique<ShulkerBoxBlock>(context, ids); }},
			{"EnderChestBlock", [context, ids](int) { return std::make_unique<EnderChestBlock>(context, ids); }},
			{"HopperBlock", [context, ids](int) { return std::make_unique<HopperBlock>(context, ids); }},
			{"DispenserBlock", [context, ids](int) { return std::make_unique<DispenserBlock>(context, ids, false); }},
			// ----- Furnaces and brewing stands -----
			{"AbstractFurnaceBlock", [context, ids](int) { return std::make_unique<AbstractFurnaceBlock>(context, ids); }},
			{"BrewingStandBlock", [context, ids](int) { return std::make_unique<BrewingStandBlock>(context, ids); }},
			// ----- End furnaces and brewing stands -----
			// ----- Container-like blocks -----
			{"ChiseledBookShelfBlock", [context, ids](int) { return std::make_unique<ChiseledBookShelfBlock>(context, ids); }},
			{"DecoratedPotBlock", [context, ids](int) { return std::make_unique<DecoratedPotBlock>(context, ids); }},
			{"JukeboxBlock", [context, ids](int) { return std::make_unique<JukeboxBlock>(context, ids); }},
			{"LecternBlock", [context, ids](int) { return std::make_unique<LecternBlock>(context, ids); }},
			{"CrafterBlock", [context, ids](int) { return std::make_unique<CrafterBlock>(context, ids); }},
			{"ShelfBlock", [context, ids](int) { return std::make_unique<ShelfBlock>(context, ids); }},
			// ----- End of container-like blocks -----
			{"ObserverBlock", [context, ids](int) { return std::make_unique<ObserverBlock>(context, ids); }},
			{"RedstoneLampBlock", [context, ids](int) { return std::make_unique<RedstoneLampBlock>(context, ids); }},
			{"PoweredBlock", [](int) { return std::make_unique<PoweredBlock>(); }},
			{"TargetBlock", [context, ids](int) { return std::make_unique<TargetBlock>(context, ids); }},

			// What comparators read
			{"CandleCakeBlock", [context](int) { return std::make_unique<AnalogOutputBlock>(context, AnalogOutputBlock::Kind::CandleCake); }},
			{"CakeBlock", [context](int) { return std::make_unique<AnalogOutputBlock>(context, AnalogOutputBlock::Kind::Cake); }},
			{"ComposterBlock", [context](int) { return std::make_unique<AnalogOutputBlock>(context, AnalogOutputBlock::Kind::Level); }},
			{"LayeredCauldronBlock", [context](int) { return std::make_unique<AnalogOutputBlock>(context, AnalogOutputBlock::Kind::Level); }},
			{"LavaCauldronBlock", [context](int) { return std::make_unique<AnalogOutputBlock>(context, AnalogOutputBlock::Kind::LavaCauldron); }},
			{"EndPortalFrameBlock", [context](int) { return std::make_unique<AnalogOutputBlock>(context, AnalogOutputBlock::Kind::EndPortalFrame); }},
			{"RespawnAnchorBlock", [context](int) { return std::make_unique<AnalogOutputBlock>(context, AnalogOutputBlock::Kind::RespawnAnchor); }},
			{"BeehiveBlock", [context](int) { return std::make_unique<AnalogOutputBlock>(context, AnalogOutputBlock::Kind::HoneyLevel); }},
			{"AmethystClusterBlock", make<AmethystClusterBlock>(context)},

			// Change by themselves
			{"KelpBlock", [context](int) { return std::make_unique<GrowingPlantBlock>(context, GrowingPlantBlock::Kind::Kelp, true); }},
			{"KelpPlantBlock", [context](int) { return std::make_unique<GrowingPlantBlock>(context, GrowingPlantBlock::Kind::Kelp, false); }},
			{"WeepingVinesBlock", [context](int) { return std::make_unique<GrowingPlantBlock>(context, GrowingPlantBlock::Kind::WeepingVines, true); }},
			{"WeepingVinesPlantBlock",
			 [context](int) { return std::make_unique<GrowingPlantBlock>(context, GrowingPlantBlock::Kind::WeepingVines, false); }},
			{"TwistingVinesBlock", [context](int) { return std::make_unique<GrowingPlantBlock>(context, GrowingPlantBlock::Kind::TwistingVines, true); }},
			{"TwistingVinesPlantBlock",
			 [context](int) { return std::make_unique<GrowingPlantBlock>(context, GrowingPlantBlock::Kind::TwistingVines, false); }},
			{"CaveVinesBlock", [context](int) { return std::make_unique<GrowingPlantBlock>(context, GrowingPlantBlock::Kind::CaveVines, true); }},
			{"CaveVinesPlantBlock", [context](int) { return std::make_unique<GrowingPlantBlock>(context, GrowingPlantBlock::Kind::CaveVines, false); }},
			{"SpreadingSnowyDirtBlock", [context](int) { return std::make_unique<SnowyDirtBlock>(context, true); }},
			{"SnowyDirtBlock", [context](int) { return std::make_unique<SnowyDirtBlock>(context, false); }},
			{"NyliumBlock", make<NyliumBlock>(context)},
			{"FarmBlock", make<FarmBlock>(context)},
			{"LeavesBlock", make<LeavesBlock>(context)},
			{"FrostedIceBlock", nullptr}, // Not ported yet
			{"IceBlock", make<IceBlock>(context)},
			{"RedStoneOreBlock", make<RedStoneOreBlock>(context)},
			{"BuddingAmethystBlock", make<BuddingAmethystBlock>(context)},
	};
	for (int block = 0; block < static_cast<int>(gameData.getBlockCount()); block++) {
		for (const auto& [javaClass, create] : classes) {
			if (!gameData.isInstanceOf(block, javaClass)) continue;
			if (create) level.behaviors().set(block, create(block));
			break;
		}
	}

	// Directional blocks without a behavior of their own still face the right way when placed
	using Rule = FacingPlacement::Rule;
	const std::vector<std::pair<const char*, Rule>> facings = {

			{"CommandBlock", Rule::NearestOpposite},	   {"AbstractFurnaceBlock", Rule::HorizontalOpposite},
			{"CarvedPumpkinBlock", Rule::HorizontalOpposite}, {"GlazedTerracottaBlock", Rule::HorizontalOpposite},
			{"LoomBlock", Rule::HorizontalOpposite},	   {"StonecutterBlock", Rule::HorizontalOpposite},
			{"ChiseledBookShelfBlock", Rule::HorizontalOpposite}, {"BeehiveBlock", Rule::HorizontalOpposite},
			{"EndPortalFrameBlock", Rule::HorizontalOpposite}, {"VaultBlock", Rule::HorizontalOpposite},
			{"AnvilBlock", Rule::HorizontalClockwise},	   

	};
	for (int block = 0; block < static_cast<int>(gameData.getBlockCount()); block++) {
		for (const auto& [javaClass, rule] : facings) {
			if (!gameData.isInstanceOf(block, javaClass)) continue;
			level.behaviors().setPlacement(block, std::make_unique<FacingPlacement>(context, rule));
			break;
		}
	}

	// Copper oxidizes, whatever the kind of block: exposed_, weathered_ then oxidized_ in front of the name
	auto ages = std::make_shared<std::vector<int8_t>>(gameData.getBlockCount(), -1);
	std::vector<int> next(gameData.getBlockCount(), -1);
	for (int block = 0; block < static_cast<int>(gameData.getBlockCount()); block++) {
		if (!gameData.isInstanceOf(block, "WeatheringCopper")) continue;
		std::string name = gameData.getStaticName("minecraft:block", block).substr(10); // Without "minecraft:"
		const char* prefixes[3] = {"exposed_", "weathered_", "oxidized_"};
		int			age			= 0;
		std::string base		= name;
		for (int i = 0; i < 3; i++) {
			if (name.rfind(prefixes[i], 0) == 0) {
				age	 = i + 1;
				base = name.substr(std::string(prefixes[i]).size());
			}
		}
		(*ages)[block] = static_cast<int8_t>(age);
		if (age == 3) continue;
		// copper_block is the unaffected exposed_copper
		if (age == 0 && base == "copper_block") base = "copper";
		next[block] = gameData.getStaticId("minecraft:block", std::string("minecraft:") + prefixes[age] + base);
	}
	for (int block = 0; block < static_cast<int>(gameData.getBlockCount()); block++) {
		if ((*ages)[block] < 0) continue;
		// Containers: copper chests only oxidize closed
		if (gameData.isInstanceOf(block, "WeatheringCopperChestBlock")) {
			level.behaviors().setRandomTick(block, std::make_unique<CopperChestWeathering>(context, next[block], ages));
			continue;
		}
		level.behaviors().setRandomTick(block, std::make_unique<WeatheringBlock>(context, next[block], ages));
	}
}
