// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SCurveEditor.h"

/** A widget for displaying and managing an SCurveEditor in sequencer. */
class SSequencerCurveEditor : public SCurveEditor
{
public:
	SLATE_BEGIN_ARGS( SSequencerCurveEditor )
	{}

		/** The range of time being viewed */
		SLATE_ATTRIBUTE( TRange<float>, ViewRange )

	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs, TSharedRef<FSequencer> InSequencer, TSharedRef<class ITimeSliderController> InTimeSliderController );

	/** Sets the sequencer node tree which supplies the curves. */
	void SetSequencerNodeTree( TSharedPtr<FSequencerNodeTree> InSequencerNodeTree );

	~SSequencerCurveEditor();

protected:

	virtual FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseMove( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseWheel( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;

private:
	/** Builds and assigns a new curve owner to the FCurveEditor. */
	void UpdateCurveOwner();

	/** Gets whether or not snapping is enabled for the curve editor. */
	bool GetCurveSnapEnabled() const;

	/** Gets time snapping interval. */
	float GetCurveTimeSnapInterval() const;

	/** Gets the value snapping interval. */
	float GetCurveValueSnapInterval() const;

	/** Gets whether or not to display curve tooltips. */
	bool GetShowCurveEditorCurveToolTips() const;

	/** Run whenever the selection on the FSequencerNodeTree changes. */
	void NodeTreeSelectionChanged();

	/** Run whenever the selected curve visibility changes. */
	void SequencerCurveVisibilityChanged();

private:

	/** The sequencer which owns this widget. */
	TWeakPtr<FSequencer> Sequencer;
	/** The class responsible for time sliding on the curve editor */
	TSharedPtr<class ITimeSliderController> TimeSliderController;
	/** The visible time range displayed by the curve editor. */
	TAttribute<TRange<float>> ViewRange;
	/** The sequencer node tree which contains the key area nodes which supply the curves to edit. */
	TSharedPtr<FSequencerNodeTree> SequencerNodeTree;
	/** The sequencer curve owner implementation which is visualized by the SCurveEditor. */
	TSharedPtr<FSequencerCurveOwner> CurveOwner;
	/** A handle to remove the node tree selection changed delegate. */
	FDelegateHandle NodeTreeSelectionChangedHandle;
};