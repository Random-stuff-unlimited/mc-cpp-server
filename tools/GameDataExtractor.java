import java.io.BufferedReader;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.lang.reflect.ParameterizedType;
import java.lang.reflect.Type;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.TreeMap;

/**
 * Reads values that only exist in the game's code (not in the data generator reports) by loading the official
 * server jar and calling its methods through reflection. Run by tools/update_gamedata.py:
 *
 *   java -cp <server jar + libraries> tools/GameDataExtractor.java <server_mappings.txt | -> <output.json>
 *
 * The jar is obfuscated: names are translated with Mojang's official mappings ("-" when the jar isn't obfuscated).
 * Output: {"blocks": {block: {property: value}}, "states": {property: [value per state id]}, "block_items": {item: block},
 *          "collision_shapes": [[[minX, minY, minZ, maxX, maxY, maxZ], ...], ...], "outline_shapes": [...]}
 * The "collision_shape" state property is an index into collision_shapes, "outline_shape" (getShape: what the cursor
 * targets) an index into outline_shapes. "classes" (per block) lists the block's class,
 * its superclasses and interfaces (simple names), so the server knows which vanilla behavior it has.
 * "entity_types" (per entity type): dimensions, tracking, category, loot table, Java classes and default attribute base
 * values; "attributes" (per attribute): default, range and whether the client is sent it.
 */
public class GameDataExtractor {
	// Deobfuscated class -> obfuscated class, and back
	private static final Map<String, String> classes = new HashMap<>();
	private static final Map<String, String> named	 = new HashMap<>();
	// "class#name(params)" or "class#field" -> obfuscated member name
	private static final Map<String, String> members = new HashMap<>();
	private static boolean obfuscated;

	public static void main(String[] args) throws Exception {
		obfuscated = !args[0].equals("-");
		if (obfuscated) readMappings(args[0]);

		invokeStatic("net.minecraft.SharedConstants", "tryDetectVersion");
		invokeStatic("net.minecraft.server.Bootstrap", "bootStrap");

		Object	 blocks	   = staticField("net.minecraft.core.registries.BuiltInRegistries", "BLOCK");
		Object	 items	   = staticField("net.minecraft.core.registries.BuiltInRegistries", "ITEM");
		Method	 getKey	   = method("net.minecraft.core.Registry", "getKey", "java.lang.Object");
		Class<?> blockItem = find("net.minecraft.world.item.BlockItem");
		Method	 getBlock  = method("net.minecraft.world.item.BlockItem", "getBlock", "");

		// Per block
		String	 block			  = "net.minecraft.world.level.block.Block";
		String[] blockProperties  = {"destroy_time", "explosion_resistance", "friction", "speed_factor", "jump_factor"};
		Method[] blockGetters	  = {method("net.minecraft.world.level.block.state.BlockBehaviour", "defaultDestroyTime", ""),
									 method(block, "getExplosionResistance", ""), method(block, "getFriction", ""),
									 method(block, "getSpeedFactor", ""), method(block, "getJumpFactor", "")};
		// Per block state, indexed by state id
		String	 stateBase		  = "net.minecraft.world.level.block.state.BlockBehaviour$BlockStateBase";
		String[] stateProperties  = {"light_emission", "requires_correct_tool", "occludes", "light_block", "propagates_skylight_down",
									 "blocks_motion", "solid", "replaceable", "random_ticking", "liquid", "solid_render",
									 "use_shape_for_light_occlusion", "ignited_by_lava", "analog_output"};
		Method[] stateGetters	  = {method(stateBase, "getLightEmission", ""), method(stateBase, "requiresCorrectToolForDrops", ""),
									 method(stateBase, "canOcclude", ""), method(stateBase, "getLightBlock", ""),
									 method(stateBase, "propagatesSkylightDown", ""), method(stateBase, "blocksMotion", ""),
									 method(stateBase, "isSolid", ""), method(stateBase, "canBeReplaced", ""),
									 method(stateBase, "isRandomlyTicking", ""), method(stateBase, "liquid", ""),
									 method(stateBase, "isSolidRender", ""), method(stateBase, "useShapeForLightOcclusion", ""),
									 method(stateBase, "ignitedByLava", ""), method(stateBase, "hasAnalogOutputSignal", "")};
		// Per block state, computed from the state in an empty world at 0, 0, 0 (the state alone decides, except for the
		// few blocks with a random offset: bamboo, dripstone...)
		String[] contextProperties = {"redstone_conductor", "face_sturdy", "collision_shape", "fluid", "fluid_amount", "fluid_falling", "occlusion_shape",
									  "push_reaction", "outline_shape", "pathfindable", "suffocating", "valid_spawn"};
		Method	 pushReaction	   = method(stateBase, "getPistonPushReaction", "");
		Method	 occlusionShape	   = method(stateBase, "getOcclusionShape", "");
		String	 getter			   = "net.minecraft.world.level.BlockGetter";
		String	 pos			   = "net.minecraft.core.BlockPos";
		Object	 emptyGetter	   = staticField("net.minecraft.world.level.EmptyBlockGetter", "INSTANCE");
		Object	 zero			   = staticField(pos, "ZERO");
		Method	 conductor		   = method(stateBase, "isRedstoneConductor", getter + "," + pos);
		Method	 pathfindable	   = method(stateBase, "isPathfindable", "net.minecraft.world.level.pathfinder.PathComputationType");
		Object[] computationTypes  = find("net.minecraft.world.level.pathfinder.PathComputationType").getEnumConstants(); // LAND, WATER, AIR
		Method	 suffocating	   = method(stateBase, "isSuffocating", getter + "," + pos);
		Method	 validSpawn		   = method(stateBase, "isValidSpawn", getter + "," + pos + ",net.minecraft.world.entity.EntityType");
		// BlockState.isValidSpawn for these types: a generic mob, ocelot, parrot, polar bear, a fire immune one (the
		// blocks' predicates only tell these apart)
		Object	 entityTypes	   = staticField("net.minecraft.core.registries.BuiltInRegistries", "ENTITY_TYPE");
		Method	 byKey			   = method("net.minecraft.core.DefaultedRegistry", "getValue", "net.minecraft.resources.ResourceLocation");
		Method	 parseKey		   = method("net.minecraft.resources.ResourceLocation", "parse", "java.lang.String");
		Object[] spawnTypes		   = new Object[5];
		String[] spawnTypeNames	   = {"zombie", "ocelot", "parrot", "polar_bear", "magma_cube"};
		for (int t = 0; t < spawnTypes.length; t++) spawnTypes[t] = byKey.invoke(entityTypes, parseKey.invoke(null, "minecraft:" + spawnTypeNames[t]));
		Method	 sturdy			   = method(stateBase, "isFaceSturdy", getter + "," + pos + ",net.minecraft.core.Direction,net.minecraft.world.level.block.SupportType");
		Object[] directions		   = find("net.minecraft.core.Direction").getEnumConstants();
		Object[] supportTypes	   = find("net.minecraft.world.level.block.SupportType").getEnumConstants(); // FULL, CENTER, RIGID
		Method	 collisionShape	   = method(stateBase, "getCollisionShape", getter + "," + pos);
		Method	 outlineShape	   = method(stateBase, "getShape", getter + "," + pos);
		Method	 toAabbs		   = method("net.minecraft.world.phys.shapes.VoxelShape", "toAabbs", "");
		String	 aabb			   = "net.minecraft.world.phys.AABB";
		Field[]	 aabbFields		   = {field(aabb, "minX"), field(aabb, "minY"), field(aabb, "minZ"), field(aabb, "maxX"), field(aabb, "maxY"), field(aabb, "maxZ")};
		Method	 fluidState		   = method(stateBase, "getFluidState", "");
		String	 fluidStateClass   = "net.minecraft.world.level.material.FluidState";
		Method	 fluidType		   = method(fluidStateClass, "getType", "");
		Method	 fluidAmount	   = method(fluidStateClass, "getAmount", "");
		String	 stateHolder	   = "net.minecraft.world.level.block.state.StateHolder";
		Method	 hasProperty	   = method(stateHolder, "hasProperty", "net.minecraft.world.level.block.state.properties.Property");
		Method	 getValue		   = method(stateHolder, "getValue", "net.minecraft.world.level.block.state.properties.Property");
		Object	 falling		   = staticField("net.minecraft.world.level.material.FlowingFluid", "FALLING");
		Object	 fluids			   = staticField("net.minecraft.core.registries.BuiltInRegistries", "FLUID");
		Method	 registryId		   = method("net.minecraft.core.IdMap", "getId", "java.lang.Object");
		Method	 dynamicShape	   = method(block, "hasDynamicShape", "");
		// The block entity type of the blocks that have one (EntityBlock.newBlockEntity at 0, 0, 0)
		Class<?> entityBlock	   = find("net.minecraft.world.level.block.EntityBlock");
		Method	 newBlockEntity	   = method("net.minecraft.world.level.block.EntityBlock", "newBlockEntity",
											"net.minecraft.core.BlockPos,net.minecraft.world.level.block.state.BlockState");
		Method	 blockEntityType   = method("net.minecraft.world.level.block.entity.BlockEntity", "getType", "");
		Object	 blockEntityTypes  = staticField("net.minecraft.core.registries.BuiltInRegistries", "BLOCK_ENTITY_TYPE");
		Method	 defaultState	   = method(block, "defaultBlockState", "");
		Map<String, Integer> shapeIds = new LinkedHashMap<>();
		Map<String, Integer> outlineShapeIds = new LinkedHashMap<>(); // Own list: the collision shape ids stay the same
		Method	 stateDefinition  = method(block, "getStateDefinition", "");
		Method	 possibleStates	  = method("net.minecraft.world.level.block.state.StateDefinition", "getPossibleStates", "");
		Method	 stateId		  = method(block, "getId", "net.minecraft.world.level.block.state.BlockState");

		StringBuilder	   blocksJson  = new StringBuilder();
		Map<Integer, Object[]> states = new TreeMap<>();
		for (Object b : (Iterable<?>) blocks) {
			if (blocksJson.length() > 0) blocksJson.append(",");
			blocksJson.append("\"").append(getKey.invoke(blocks, b)).append("\":{");
			for (int i = 0; i < blockProperties.length; i++) {
				blocksJson.append(i > 0 ? "," : "").append("\"").append(blockProperties[i]).append("\":").append(blockGetters[i].invoke(b));
			}
			blocksJson.append(",\"dynamic_shape\":").append(dynamicShape.invoke(b));
			if (entityBlock.isInstance(b)) {
				Object entity = newBlockEntity.invoke(b, zero, defaultState.invoke(b));
				if (entity != null) blocksJson.append(",\"block_entity\":\"").append(getKey.invoke(blockEntityTypes, blockEntityType.invoke(entity))).append("\"");
			}
			blocksJson.append(",\"classes\":[");
			int c = 0;
			for (String name : classNames(b.getClass())) blocksJson.append(c++ > 0 ? "," : "").append("\"").append(name).append("\"");
			blocksJson.append("]}");
			for (Object state : (Iterable<?>) possibleStates.invoke(stateDefinition.invoke(b))) {
				Object[] values = new Object[stateGetters.length + contextProperties.length];
				for (int i = 0; i < stateGetters.length; i++) values[i] = stateGetters[i].invoke(state);
				int n = stateGetters.length;
				values[n++] = conductor.invoke(state, emptyGetter, zero);
				int faces	= 0; // Bit direction * 3 + support type
				for (int d = 0; d < directions.length; d++) {
					for (int t = 0; t < supportTypes.length; t++) {
						if ((Boolean) sturdy.invoke(state, emptyGetter, zero, directions[d], supportTypes[t])) faces |= 1 << (d * 3 + t);
					}
				}
				values[n++] = faces;
				values[n++] = shapeIds.computeIfAbsent(boxesOf(toAabbs, aabbFields, collisionShape.invoke(state, emptyGetter, zero)), k -> shapeIds.size());
				Object fluid = fluidState.invoke(state);
				values[n++]	 = registryId.invoke(fluids, fluidType.invoke(fluid));
				values[n++]	 = fluidAmount.invoke(fluid);
				values[n++]	 = (Boolean) hasProperty.invoke(fluid, falling) && (Boolean) getValue.invoke(fluid, falling);
				values[n++]	 = shapeIds.computeIfAbsent(boxesOf(toAabbs, aabbFields, occlusionShape.invoke(state)), k -> shapeIds.size());
				values[n++]	 = ((Enum<?>) pushReaction.invoke(state)).ordinal(); // NORMAL, DESTROY, BLOCK, IGNORE, PUSH_ONLY
				values[n++]	 = outlineShapeIds.computeIfAbsent(boxesOf(toAabbs, aabbFields, outlineShape.invoke(state, emptyGetter, zero)), k -> outlineShapeIds.size());
				int paths	 = 0; // Bit per PathComputationType (LAND 1, WATER 2, AIR 4)
				for (int t = 0; t < computationTypes.length; t++) {
					if ((Boolean) pathfindable.invoke(state, computationTypes[t])) paths |= 1 << t;
				}
				values[n++] = paths;
				values[n++] = suffocating.invoke(state, emptyGetter, zero);
				int spawns	= 0; // Bit per type of spawnTypes
				for (int t = 0; t < spawnTypes.length; t++) {
					if ((Boolean) validSpawn.invoke(state, emptyGetter, zero, spawnTypes[t])) spawns |= 1 << t;
				}
				values[n++] = spawns;
				states.put((Integer) stateId.invoke(null, state), values);
			}
		}

		StringBuilder statesJson = new StringBuilder();
		List<String>  allProperties = new ArrayList<>(List.of(stateProperties));
		allProperties.addAll(List.of(contextProperties));
		for (int i = 0; i < allProperties.size(); i++) {
			statesJson.append(i > 0 ? "," : "").append("\"").append(allProperties.get(i)).append("\":[");
			int n = 0;
			for (Object[] values : states.values()) statesJson.append(n++ > 0 ? "," : "").append(values[i]);
			statesJson.append("]");
		}

		// What stays after crafting with an item (Item.getCraftingRemainder: buckets, bottles...)
		Method				remainder		  = method("net.minecraft.world.item.Item", "getCraftingRemainder", "");
		Method				stackItem		  = method("net.minecraft.world.item.ItemStack", "getItem", "");
		Method				stackEmpty		  = method("net.minecraft.world.item.ItemStack", "isEmpty", "");
		Map<String, String> craftingRemainders = new TreeMap<>();
		for (Object item : (Iterable<?>) items) {
			Object stack = remainder.invoke(item);
			if (stack != null && !(Boolean) stackEmpty.invoke(stack)) {
				craftingRemainders.put(getKey.invoke(items, item).toString(), getKey.invoke(items, stackItem.invoke(stack)).toString());
			}
		}

		Map<String, String> blockItems = new TreeMap<>();
		for (Object item : (Iterable<?>) items) {
			if (!blockItem.isInstance(item)) continue;
			blockItems.put(getKey.invoke(items, item).toString(), getKey.invoke(blocks, getBlock.invoke(item)).toString());
		}

		try (FileWriter out = new FileWriter(args[1])) {
			out.write("{\"blocks\":{" + blocksJson + "},\"states\":{" + statesJson + "},\"block_items\":{");
			writeEntries(out, blockItems);
			out.write("},\"crafting_remainders\":{");
			writeEntries(out, craftingRemainders);
			out.write("},\"collision_shapes\":[" + String.join(",", shapeIds.keySet()) + "]");
			out.write(",\"outline_shapes\":[" + String.join(",", outlineShapeIds.keySet()) + "],");
			out.write(entityTypes() + "}\n");
		}
		System.out.println("Extracted " + states.size() + " block states and " + blockItems.size() + " block items");
		System.exit(0); // The game leaves non-daemon threads running
	}

	// Entity types (EntityType and DefaultAttributes) and attributes (Attribute, RangedAttribute), as two JSON members
	private static String entityTypes() throws Exception {
		Object	 types		  = staticField("net.minecraft.core.registries.BuiltInRegistries", "ENTITY_TYPE");
		Object	 attributes	  = staticField("net.minecraft.core.registries.BuiltInRegistries", "ATTRIBUTE");
		Method	 getKey		  = method("net.minecraft.core.Registry", "getKey", "java.lang.Object");
		Method	 wrapAsHolder = method("net.minecraft.core.Registry", "wrapAsHolder", "java.lang.Object");
		String	 type		  = "net.minecraft.world.entity.EntityType";
		Method	 dimensions	  = method(type, "getDimensions", "");
		Method[] typeGetters  = {method(type, "clientTrackingRange", ""), method(type, "updateInterval", ""), method(type, "trackDeltas", ""),
								 method(type, "fireImmune", ""), method(type, "canSummon", ""), method(type, "canSerialize", ""),
								 method(type, "isAllowedInPeaceful", "")};
		String[] typeNames	  = {"tracking_range", "update_interval", "track_deltas", "fire_immune", "summonable", "serializable", "allowed_in_peaceful"};
		Method	 category	  = method(type, "getCategory", "");
		Method	 lootTable	  = method(type, "getDefaultLootTable", "");
		Method	 location	  = method("net.minecraft.resources.ResourceKey", "location", "");
		String	 dims		  = "net.minecraft.world.entity.EntityDimensions";
		Method[] dimGetters	  = {method(dims, "width", ""), method(dims, "height", ""), method(dims, "eyeHeight", ""), method(dims, "fixed", "")};
		String[] dimNames	  = {"width", "height", "eye_height", "fixed"};
		String	 defaults	  = "net.minecraft.world.entity.ai.attributes.DefaultAttributes";
		Method	 hasSupplier  = method(defaults, "hasSupplier", type);
		Method	 getSupplier  = method(defaults, "getSupplier", type);
		String	 supplier	  = "net.minecraft.world.entity.ai.attributes.AttributeSupplier";
		Method	 hasAttribute = method(supplier, "hasAttribute", "net.minecraft.core.Holder");
		Method	 baseValue	  = method(supplier, "getBaseValue", "net.minecraft.core.Holder");
		String	 attribute	  = "net.minecraft.world.entity.ai.attributes.Attribute";
		Method	 defaultValue = method(attribute, "getDefaultValue", "");
		Method	 syncable	  = method(attribute, "isClientSyncable", "");
		Class<?> ranged		  = find("net.minecraft.world.entity.ai.attributes.RangedAttribute");
		Method	 minValue	  = method("net.minecraft.world.entity.ai.attributes.RangedAttribute", "getMinValue", "");
		Method	 maxValue	  = method("net.minecraft.world.entity.ai.attributes.RangedAttribute", "getMaxValue", "");

		// The entity class of each type, from the generic type of its EntityType field (EntityType<Cow> COW)
		Map<Object, Class<?>> classes = new HashMap<>();
		for (Field field : find(type).getDeclaredFields()) {
			if (!Modifier.isStatic(field.getModifiers()) || field.getType() != find(type)) continue;
			Type generic = field.getGenericType();
			if (!(generic instanceof ParameterizedType parameterized) || !(parameterized.getActualTypeArguments()[0] instanceof Class<?> c)) continue;
			field.setAccessible(true);
			classes.put(field.get(null), c);
		}

		StringBuilder json = new StringBuilder("\"attributes\":{");
		List<Object[]> holders = new ArrayList<>(); // Name, holder
		for (Object a : (Iterable<?>) attributes) {
			String name = getKey.invoke(attributes, a).toString();
			holders.add(new Object[] {name, wrapAsHolder.invoke(attributes, a)});
			json.append(holders.size() > 1 ? "," : "").append("\"").append(name).append("\":{\"default\":").append(defaultValue.invoke(a));
			if (ranged.isInstance(a)) json.append(",\"min\":").append(minValue.invoke(a)).append(",\"max\":").append(maxValue.invoke(a));
			json.append(",\"syncable\":").append(syncable.invoke(a)).append("}");
		}
		json.append("},\"entity_types\":{");
		int n = 0;
		for (Object t : (Iterable<?>) types) {
			json.append(n++ > 0 ? "," : "").append("\"").append(getKey.invoke(types, t)).append("\":{");
			Object d = dimensions.invoke(t);
			for (int i = 0; i < dimGetters.length; i++) json.append(i > 0 ? "," : "").append("\"").append(dimNames[i]).append("\":").append(dimGetters[i].invoke(d));
			for (int i = 0; i < typeGetters.length; i++) json.append(",\"").append(typeNames[i]).append("\":").append(typeGetters[i].invoke(t));
			json.append(",\"category\":\"").append(((Enum<?>) category.invoke(t)).name().toLowerCase()).append("\"");
			Object loot = ((java.util.Optional<?>) lootTable.invoke(t)).orElse(null);
			if (loot != null) json.append(",\"loot_table\":\"").append(location.invoke(loot)).append("\"");
			Class<?> c = classes.get(t);
			if (c != null) {
				json.append(",\"classes\":[");
				int i = 0;
				for (String name : entityClassNames(c)) json.append(i++ > 0 ? "," : "").append("\"").append(name).append("\"");
				json.append("]");
			}
			if ((Boolean) hasSupplier.invoke(null, t)) {
				Object s = getSupplier.invoke(null, t);
				json.append(",\"attributes\":{");
				int i = 0;
				for (Object[] holder : holders) {
					if (!(Boolean) hasAttribute.invoke(s, holder[1])) continue;
					json.append(i++ > 0 ? "," : "").append("\"").append(holder[0]).append("\":").append(baseValue.invoke(s, holder[1]));
				}
				json.append("}");
			}
			json.append("}");
		}
		return json.append("}").toString();
	}

	// The class, its superclasses up to Entity, and every interface along the way, as simple deobfuscated names
	private static Set<String> entityClassNames(Class<?> type) {
		Set<String> names = new LinkedHashSet<>();
		for (Class<?> c = type; c != null && !c.getName().equals("java.lang.Object"); c = c.getSuperclass()) {
			names.add(simpleName(c));
			for (Class<?> i : c.getInterfaces()) names.add(simpleName(i));
			if (simpleName(c).equals("Entity")) break;
		}
		return names;
	}

	private static void writeEntries(FileWriter out, Map<String, ?> entries) throws IOException {
		boolean first = true;
		for (Map.Entry<String, ?> e : entries.entrySet()) {
			if (!first) out.write(",");
			first = false;
			Object value = e.getValue();
			out.write("\"" + e.getKey() + "\":" + (value instanceof String ? "\"" + value + "\"" : value.toString()));
		}
	}

	// ===== Mappings =====

	private static void readMappings(String path) throws IOException {
		try (BufferedReader in = new BufferedReader(new FileReader(path))) {
			String line;
			String current = null;
			while ((line = in.readLine()) != null) {
				if (line.startsWith("#") || line.isBlank()) continue;
				if (!line.startsWith(" ")) {
					// "net.minecraft.Foo -> abc:"
					String[] parts = line.split(" -> ");
					current		   = parts[0];
					classes.put(current, parts[1].substring(0, parts[1].length() - 1));
					named.put(parts[1].substring(0, parts[1].length() - 1), current);
					continue;
				}
				// "    12:34:void name(int,java.lang.String) -> a" or "    int field -> b"
				String[] parts	   = line.trim().split(" -> ");
				String	 signature = parts[0].replaceFirst("^\\d+:\\d+:", "");
				String	 name	   = signature.substring(signature.indexOf(' ') + 1);
				members.put(current + "#" + name, parts[1]);
			}
		}
	}

	private static Class<?> find(String name) throws ClassNotFoundException {
		String obf = obfuscated ? classes.get(name) : name;
		if (obf == null) throw new IllegalStateException("No mapping for class " + name);
		return Class.forName(obf);
	}

	private static String member(String owner, String key, String name) {
		if (!obfuscated) return name;
		String obf = members.get(owner + "#" + key);
		if (obf == null) throw new IllegalStateException("No mapping for " + owner + "#" + key);
		return obf;
	}

	// Parameters as written in the mappings, e.g. "java.lang.Object" or "". Obfuscation gives overloads the same
	// name, so the parameter types have to match too
	private static Method method(String owner, String name, String params) throws Exception {
		String	 obfName = member(owner, name + "(" + params + ")", name);
		String[] wanted	 = params.isEmpty() ? new String[0] : params.split(",");
		for (int i = 0; i < wanted.length; i++) {
			String base	  = wanted[i].replace("[]", "");
			String mapped = obfuscated && classes.containsKey(base) ? classes.get(base) : base;
			wanted[i]	  = mapped + wanted[i].substring(base.length());
		}
		for (Class<?> c = find(owner); c != null; c = c.getSuperclass()) {
			for (Method m : c.getDeclaredMethods()) {
				if (!m.getName().equals(obfName) || m.getParameterCount() != wanted.length) continue;
				boolean match = true;
				for (int i = 0; i < wanted.length; i++) match &= m.getParameterTypes()[i].getTypeName().equals(wanted[i]);
				if (!match) continue;
				m.setAccessible(true);
				return m;
			}
		}
		throw new NoSuchMethodException(owner + "." + name + "(" + params + ")");
	}

	private static void invokeStatic(String owner, String name) throws Exception { method(owner, name, "").invoke(null); }

	// A VoxelShape as JSON boxes: [[minX, minY, minZ, maxX, maxY, maxZ], ...]
	private static String boxesOf(Method toAabbs, Field[] aabbFields, Object shape) throws Exception {
		StringBuilder boxes = new StringBuilder("[");
		for (Object box : (List<?>) toAabbs.invoke(shape)) {
			boxes.append(boxes.length() > 1 ? "," : "").append("[");
			for (int i = 0; i < 6; i++) boxes.append(i > 0 ? "," : "").append(aabbFields[i].getDouble(box));
			boxes.append("]");
		}
		return boxes.append("]").toString();
	}

	private static Field field(String owner, String name) throws Exception {
		Field field = find(owner).getDeclaredField(member(owner, name, name));
		field.setAccessible(true);
		return field;
	}

	// The class, its superclasses up to Block, and every interface along the way, as simple deobfuscated names
	private static Set<String> classNames(Class<?> type) {
		Set<String> names = new LinkedHashSet<>();
		for (Class<?> c = type; c != null && !c.getName().equals("java.lang.Object"); c = c.getSuperclass()) {
			names.add(simpleName(c));
			for (Class<?> i : c.getInterfaces()) names.add(simpleName(i));
			if (simpleName(c).equals("Block")) break;
		}
		return names;
	}

	private static String simpleName(Class<?> c) {
		String name = obfuscated ? named.getOrDefault(c.getName(), c.getName()) : c.getName();
		return name.substring(Math.max(name.lastIndexOf('.'), name.lastIndexOf('$')) + 1);
	}

	private static Object staticField(String owner, String name) throws Exception {
		Field field = find(owner).getDeclaredField(member(owner, name, name));
		field.setAccessible(true);
		return field.get(null);
	}
}
