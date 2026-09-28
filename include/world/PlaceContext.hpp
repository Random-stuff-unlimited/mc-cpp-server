#ifndef PLACE_CONTEXT_HPP
#define PLACE_CONTEXT_HPP

#include "world/BlockPos.hpp"

#include <array>

// Vanilla's BlockPlaceContext: a player placing a block by clicking a face of another one
struct PlaceContext {
	BlockPos  clickedPos;	   // Where the block goes (getClickedPos)
	Direction clickedFace;	   // Face of the block clicked
	double	  clickX, clickY, clickZ; // Where the click hit, in world coordinates
	bool	  replaceClicked;  // Placed into the clicked block (tall grass...), not next to it
	float	  yaw, pitch;	   // The player's view
	bool	  secondaryUse;	   // Sneaking
	int		  item;			   // Item placed
	int		  block;		   // Block that item places (minecraft:block id)

	// Entity.getDirection: the horizontal direction the player faces
	Direction horizontalDirection() const;
	// Direction.orderedByNearest: the six directions, closest to the view first
	std::array<Direction, 6> orderedByNearest() const;
	Direction				 nearestLookingDirection() const { return orderedByNearest()[0]; }
	// getNearestLookingDirections: the same, but the clicked face's opposite first when placing next to it
	std::array<Direction, 6> nearestLookingDirections() const;
	// Direction.getFacingAxis(player, Y)
	Direction nearestLookingVerticalDirection() const { return pitch < 0.0F ? Direction::Up : Direction::Down; }
};

#endif
