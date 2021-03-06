// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/feedback-vector.h"
#include "src/code-stubs.h"
#include "src/feedback-vector-inl.h"
#include "src/ic/ic-inl.h"
#include "src/objects.h"
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

FeedbackSlot FeedbackVectorSpec::AddSlot(FeedbackSlotKind kind) {
  int slot = slots();
  int entries_per_slot = FeedbackMetadata::GetSlotSize(kind);
  append(kind);
  for (int i = 1; i < entries_per_slot; i++) {
    append(FeedbackSlotKind::kInvalid);
  }
  return FeedbackSlot(slot);
}

FeedbackSlot FeedbackVectorSpec::AddTypeProfileSlot() {
  FeedbackSlot slot = AddSlot(FeedbackSlotKind::kTypeProfile);
  CHECK_EQ(FeedbackVectorSpec::kTypeProfileSlotIndex,
           FeedbackVector::GetIndex(slot));
  return slot;
}

bool FeedbackVectorSpec::HasTypeProfileSlot() const {
  FeedbackSlot slot =
      FeedbackVector::ToSlot(FeedbackVectorSpec::kTypeProfileSlotIndex);
  if (slots() <= slot.ToInt()) {
    return false;
  }
  return GetKind(slot) == FeedbackSlotKind::kTypeProfile;
}

static bool IsPropertyNameFeedback(Object* feedback) {
  if (feedback->IsString()) return true;
  if (!feedback->IsSymbol()) return false;
  Symbol* symbol = Symbol::cast(feedback);
  Heap* heap = symbol->GetHeap();
  return symbol != heap->uninitialized_symbol() &&
         symbol != heap->premonomorphic_symbol() &&
         symbol != heap->megamorphic_symbol();
}

std::ostream& operator<<(std::ostream& os, FeedbackSlotKind kind) {
  return os << FeedbackMetadata::Kind2String(kind);
}

FeedbackSlotKind FeedbackMetadata::GetKind(FeedbackSlot slot) const {
  int index = VectorICComputer::index(0, slot.ToInt());
  int data = get(index);
  return VectorICComputer::decode(data, slot.ToInt());
}

void FeedbackMetadata::SetKind(FeedbackSlot slot, FeedbackSlotKind kind) {
  int index = VectorICComputer::index(0, slot.ToInt());
  int data = get(index);
  int new_data = VectorICComputer::encode(data, slot.ToInt(), kind);
  set(index, new_data);
}

// static
Handle<FeedbackMetadata> FeedbackMetadata::New(Isolate* isolate,
                                               const FeedbackVectorSpec* spec) {
  Factory* factory = isolate->factory();

  const int slot_count = spec == nullptr ? 0 : spec->slots();
  if (slot_count == 0) {
    return factory->empty_feedback_metadata();
  }
#ifdef DEBUG
  for (int i = 0; i < slot_count;) {
    DCHECK(spec);
    FeedbackSlotKind kind = spec->GetKind(FeedbackSlot(i));
    int entry_size = FeedbackMetadata::GetSlotSize(kind);
    for (int j = 1; j < entry_size; j++) {
      FeedbackSlotKind kind = spec->GetKind(FeedbackSlot(i + j));
      DCHECK_EQ(FeedbackSlotKind::kInvalid, kind);
    }
    i += entry_size;
  }
#endif

  Handle<FeedbackMetadata> metadata = factory->NewFeedbackMetadata(slot_count);

  // Initialize the slots. The raw data section has already been pre-zeroed in
  // NewFeedbackMetadata.
  for (int i = 0; i < slot_count; i++) {
    DCHECK(spec);
    FeedbackSlot slot(i);
    FeedbackSlotKind kind = spec->GetKind(slot);
    metadata->SetKind(slot, kind);
  }

  return metadata;
}

bool FeedbackMetadata::SpecDiffersFrom(
    const FeedbackVectorSpec* other_spec) const {
  if (other_spec->slots() != slot_count()) {
    return true;
  }

  int slots = slot_count();
  for (int i = 0; i < slots;) {
    FeedbackSlot slot(i);
    FeedbackSlotKind kind = GetKind(slot);
    int entry_size = FeedbackMetadata::GetSlotSize(kind);

    if (kind != other_spec->GetKind(slot)) {
      return true;
    }
    i += entry_size;
  }
  return false;
}

const char* FeedbackMetadata::Kind2String(FeedbackSlotKind kind) {
  switch (kind) {
    case FeedbackSlotKind::kInvalid:
      return "Invalid";
    case FeedbackSlotKind::kCall:
      return "Call";
    case FeedbackSlotKind::kLoadProperty:
      return "LoadProperty";
    case FeedbackSlotKind::kLoadGlobalInsideTypeof:
      return "LoadGlobalInsideTypeof";
    case FeedbackSlotKind::kLoadGlobalNotInsideTypeof:
      return "LoadGlobalNotInsideTypeof";
    case FeedbackSlotKind::kLoadKeyed:
      return "LoadKeyed";
    case FeedbackSlotKind::kStoreNamedSloppy:
      return "StoreNamedSloppy";
    case FeedbackSlotKind::kStoreNamedStrict:
      return "StoreNamedStrict";
    case FeedbackSlotKind::kStoreOwnNamed:
      return "StoreOwnNamed";
    case FeedbackSlotKind::kStoreGlobalSloppy:
      return "StoreGlobalSloppy";
    case FeedbackSlotKind::kStoreGlobalStrict:
      return "StoreGlobalStrict";
    case FeedbackSlotKind::kStoreKeyedSloppy:
      return "StoreKeyedSloppy";
    case FeedbackSlotKind::kStoreKeyedStrict:
      return "StoreKeyedStrict";
    case FeedbackSlotKind::kStoreInArrayLiteral:
      return "StoreInArrayLiteral";
    case FeedbackSlotKind::kBinaryOp:
      return "BinaryOp";
    case FeedbackSlotKind::kCompareOp:
      return "CompareOp";
    case FeedbackSlotKind::kStoreDataPropertyInLiteral:
      return "StoreDataPropertyInLiteral";
    case FeedbackSlotKind::kCreateClosure:
      return "kCreateClosure";
    case FeedbackSlotKind::kLiteral:
      return "Literal";
    case FeedbackSlotKind::kTypeProfile:
      return "TypeProfile";
    case FeedbackSlotKind::kForIn:
      return "ForIn";
    case FeedbackSlotKind::kInstanceOf:
      return "InstanceOf";
    case FeedbackSlotKind::kKindsNumber:
      break;
  }
  UNREACHABLE();
}

bool FeedbackMetadata::HasTypeProfileSlot() const {
  FeedbackSlot slot =
      FeedbackVector::ToSlot(FeedbackVectorSpec::kTypeProfileSlotIndex);
  return slot.ToInt() < slot_count() &&
         GetKind(slot) == FeedbackSlotKind::kTypeProfile;
}

FeedbackSlotKind FeedbackVector::GetKind(FeedbackSlot slot) const {
  DCHECK(!is_empty());
  return metadata()->GetKind(slot);
}

FeedbackSlot FeedbackVector::GetTypeProfileSlot() const {
  DCHECK(metadata()->HasTypeProfileSlot());
  FeedbackSlot slot =
      FeedbackVector::ToSlot(FeedbackVectorSpec::kTypeProfileSlotIndex);
  DCHECK_EQ(FeedbackSlotKind::kTypeProfile, GetKind(slot));
  return slot;
}

// static
Handle<FeedbackVector> FeedbackVector::New(Isolate* isolate,
                                           Handle<SharedFunctionInfo> shared) {
  Factory* factory = isolate->factory();

  const int slot_count = shared->feedback_metadata()->slot_count();

  Handle<FeedbackVector> vector = factory->NewFeedbackVector(shared, TENURED);

  DCHECK_EQ(vector->length(), slot_count);

  DCHECK_EQ(vector->shared_function_info(), *shared);
  DCHECK_EQ(
      vector->optimized_code_weak_or_smi(),
      MaybeObject::FromSmi(Smi::FromEnum(
          FLAG_log_function_events ? OptimizationMarker::kLogFirstExecution
                                   : OptimizationMarker::kNone)));
  DCHECK_EQ(vector->invocation_count(), 0);
  DCHECK_EQ(vector->profiler_ticks(), 0);
  DCHECK_EQ(vector->deopt_count(), 0);

  // Ensure we can skip the write barrier
  Handle<Object> uninitialized_sentinel = UninitializedSentinel(isolate);
  DCHECK_EQ(isolate->heap()->uninitialized_symbol(), *uninitialized_sentinel);
  Handle<Oddball> undefined_value = factory->undefined_value();
  for (int i = 0; i < slot_count;) {
    FeedbackSlot slot(i);
    FeedbackSlotKind kind = shared->feedback_metadata()->GetKind(slot);
    int index = FeedbackVector::GetIndex(slot);
    int entry_size = FeedbackMetadata::GetSlotSize(kind);

    Object* extra_value = *uninitialized_sentinel;
    switch (kind) {
      case FeedbackSlotKind::kLoadGlobalInsideTypeof:
      case FeedbackSlotKind::kLoadGlobalNotInsideTypeof:
      case FeedbackSlotKind::kStoreGlobalSloppy:
      case FeedbackSlotKind::kStoreGlobalStrict:
        vector->set(index, isolate->heap()->empty_weak_cell(),
                    SKIP_WRITE_BARRIER);
        break;
      case FeedbackSlotKind::kForIn:
      case FeedbackSlotKind::kCompareOp:
      case FeedbackSlotKind::kBinaryOp:
        vector->set(index, Smi::kZero, SKIP_WRITE_BARRIER);
        break;
      case FeedbackSlotKind::kCreateClosure: {
        Handle<FeedbackCell> cell = factory->NewNoClosuresCell(undefined_value);
        vector->set(index, *cell);
        break;
      }
      case FeedbackSlotKind::kLiteral:
        vector->set(index, Smi::kZero, SKIP_WRITE_BARRIER);
        break;
      case FeedbackSlotKind::kCall:
        vector->set(index, *uninitialized_sentinel, SKIP_WRITE_BARRIER);
        extra_value = Smi::kZero;
        break;
      case FeedbackSlotKind::kLoadProperty:
      case FeedbackSlotKind::kLoadKeyed:
      case FeedbackSlotKind::kStoreNamedSloppy:
      case FeedbackSlotKind::kStoreNamedStrict:
      case FeedbackSlotKind::kStoreOwnNamed:
      case FeedbackSlotKind::kStoreKeyedSloppy:
      case FeedbackSlotKind::kStoreKeyedStrict:
      case FeedbackSlotKind::kStoreInArrayLiteral:
      case FeedbackSlotKind::kStoreDataPropertyInLiteral:
      case FeedbackSlotKind::kTypeProfile:
      case FeedbackSlotKind::kInstanceOf:
        vector->set(index, *uninitialized_sentinel, SKIP_WRITE_BARRIER);
        break;

      case FeedbackSlotKind::kInvalid:
      case FeedbackSlotKind::kKindsNumber:
        UNREACHABLE();
        break;
    }
    for (int j = 1; j < entry_size; j++) {
      vector->set(index + j, extra_value, SKIP_WRITE_BARRIER);
    }
    i += entry_size;
  }

  Handle<FeedbackVector> result = Handle<FeedbackVector>::cast(vector);
  if (!isolate->is_best_effort_code_coverage() ||
      isolate->is_collecting_type_profile()) {
    AddToVectorsForProfilingTools(isolate, result);
  }
  return result;
}

// static
void FeedbackVector::AddToVectorsForProfilingTools(
    Isolate* isolate, Handle<FeedbackVector> vector) {
  DCHECK(!isolate->is_best_effort_code_coverage() ||
         isolate->is_collecting_type_profile());
  if (!vector->shared_function_info()->IsSubjectToDebugging()) return;
  Handle<ArrayList> list = Handle<ArrayList>::cast(
      isolate->factory()->feedback_vectors_for_profiling_tools());
  list = ArrayList::Add(list, vector);
  isolate->SetFeedbackVectorsForProfilingTools(*list);
}

// static
void FeedbackVector::SetOptimizedCode(Handle<FeedbackVector> vector,
                                      Handle<Code> code) {
  DCHECK_EQ(code->kind(), Code::OPTIMIZED_FUNCTION);
  vector->set_optimized_code_weak_or_smi(HeapObjectReference::Weak(*code));
}

void FeedbackVector::ClearOptimizedCode() {
  DCHECK(has_optimized_code());
  SetOptimizationMarker(OptimizationMarker::kNone);
}

void FeedbackVector::ClearOptimizationMarker() {
  DCHECK(!has_optimized_code());
  SetOptimizationMarker(OptimizationMarker::kNone);
}

void FeedbackVector::SetOptimizationMarker(OptimizationMarker marker) {
  set_optimized_code_weak_or_smi(MaybeObject::FromSmi(Smi::FromEnum(marker)));
}

void FeedbackVector::EvictOptimizedCodeMarkedForDeoptimization(
    SharedFunctionInfo* shared, const char* reason) {
  MaybeObject* slot = optimized_code_weak_or_smi();
  if (slot->IsSmi()) {
    return;
  }

  if (slot->IsClearedWeakHeapObject()) {
    ClearOptimizationMarker();
    return;
  }

  Code* code = Code::cast(slot->GetHeapObject());
  if (code->marked_for_deoptimization()) {
    if (FLAG_trace_deopt) {
      PrintF("[evicting optimizing code marked for deoptimization (%s) for ",
             reason);
      shared->ShortPrint();
      PrintF("]\n");
    }
    if (!code->deopt_already_counted()) {
      increment_deopt_count();
      code->set_deopt_already_counted(true);
    }
    ClearOptimizedCode();
  }
}

bool FeedbackVector::ClearSlots(Isolate* isolate) {
  Object* uninitialized_sentinel =
      FeedbackVector::RawUninitializedSentinel(isolate);

  bool feedback_updated = false;
  FeedbackMetadataIterator iter(metadata());
  while (iter.HasNext()) {
    FeedbackSlot slot = iter.Next();

    Object* obj = Get(slot)->ToObject();
    if (obj != uninitialized_sentinel) {
      FeedbackNexus nexus(this, slot);
      feedback_updated |= nexus.Clear();
    }
  }
  return feedback_updated;
}

void FeedbackVector::AssertNoLegacyTypes(Object* object) {
  // Instead of FixedArray, the Feedback and the Extra should contain
  // WeakFixedArrays. The only allowed FixedArray subtype is HashTable.
  DCHECK_IMPLIES(object->IsFixedArray(), object->IsHashTable());
}

Handle<WeakFixedArray> FeedbackNexus::EnsureArrayOfSize(int length) {
  Isolate* isolate = GetIsolate();
  Handle<Object> feedback = handle(GetFeedback(), isolate);
  if (!feedback->IsWeakFixedArray() ||
      WeakFixedArray::cast(*feedback)->length() != length) {
    Handle<WeakFixedArray> array =
        isolate->factory()->NewWeakFixedArray(length);
    SetFeedback(*array);
    return array;
  }
  return Handle<WeakFixedArray>::cast(feedback);
}

Handle<WeakFixedArray> FeedbackNexus::EnsureExtraArrayOfSize(int length) {
  Isolate* isolate = GetIsolate();
  HeapObject* heap_object;
  if (GetFeedbackExtra()->ToStrongHeapObject(&heap_object) &&
      heap_object->IsWeakFixedArray() &&
      WeakFixedArray::cast(heap_object)->length() == length) {
    return handle(WeakFixedArray::cast(heap_object));
  }
  Handle<WeakFixedArray> array = isolate->factory()->NewWeakFixedArray(length);
  SetFeedbackExtra(*array);
  return array;
}

void FeedbackNexus::ConfigureUninitialized() {
  Isolate* isolate = GetIsolate();
  switch (kind()) {
    case FeedbackSlotKind::kStoreGlobalSloppy:
    case FeedbackSlotKind::kStoreGlobalStrict:
    case FeedbackSlotKind::kLoadGlobalNotInsideTypeof:
    case FeedbackSlotKind::kLoadGlobalInsideTypeof: {
      SetFeedback(isolate->heap()->empty_weak_cell(), SKIP_WRITE_BARRIER);
      SetFeedbackExtra(*FeedbackVector::UninitializedSentinel(isolate),
                       SKIP_WRITE_BARRIER);
      break;
    }
    case FeedbackSlotKind::kCall: {
      SetFeedback(*FeedbackVector::UninitializedSentinel(isolate),
                  SKIP_WRITE_BARRIER);
      SetFeedbackExtra(Smi::kZero, SKIP_WRITE_BARRIER);
      break;
    }
    case FeedbackSlotKind::kInstanceOf: {
      SetFeedback(*FeedbackVector::UninitializedSentinel(isolate),
                  SKIP_WRITE_BARRIER);
      break;
    }
    case FeedbackSlotKind::kStoreDataPropertyInLiteral: {
      SetFeedback(*FeedbackVector::UninitializedSentinel(isolate),
                  SKIP_WRITE_BARRIER);
      SetFeedbackExtra(*FeedbackVector::UninitializedSentinel(isolate),
                       SKIP_WRITE_BARRIER);
      break;
    }
    default:
      UNREACHABLE();
  }
}

bool FeedbackNexus::Clear() {
  bool feedback_updated = false;

  switch (kind()) {
    case FeedbackSlotKind::kCreateClosure:
    case FeedbackSlotKind::kTypeProfile:
      // We don't clear these kinds ever.
      break;

    case FeedbackSlotKind::kCompareOp:
    case FeedbackSlotKind::kForIn:
    case FeedbackSlotKind::kBinaryOp:
      // We don't clear these, either.
      break;

    case FeedbackSlotKind::kLiteral:
      SetFeedback(Smi::kZero, SKIP_WRITE_BARRIER);
      feedback_updated = true;
      break;

    case FeedbackSlotKind::kStoreNamedSloppy:
    case FeedbackSlotKind::kStoreNamedStrict:
    case FeedbackSlotKind::kStoreKeyedSloppy:
    case FeedbackSlotKind::kStoreKeyedStrict:
    case FeedbackSlotKind::kStoreInArrayLiteral:
    case FeedbackSlotKind::kStoreOwnNamed:
    case FeedbackSlotKind::kLoadProperty:
    case FeedbackSlotKind::kLoadKeyed:
      if (!IsCleared()) {
        ConfigurePremonomorphic();
        feedback_updated = true;
      }
      break;

    case FeedbackSlotKind::kStoreGlobalSloppy:
    case FeedbackSlotKind::kStoreGlobalStrict:
    case FeedbackSlotKind::kLoadGlobalNotInsideTypeof:
    case FeedbackSlotKind::kLoadGlobalInsideTypeof:
    case FeedbackSlotKind::kCall:
    case FeedbackSlotKind::kInstanceOf:
    case FeedbackSlotKind::kStoreDataPropertyInLiteral:
      if (!IsCleared()) {
        ConfigureUninitialized();
        feedback_updated = true;
      }
      break;

    case FeedbackSlotKind::kInvalid:
    case FeedbackSlotKind::kKindsNumber:
      UNREACHABLE();
      break;
  }
  return feedback_updated;
}

void FeedbackNexus::ConfigurePremonomorphic() {
  SetFeedback(*FeedbackVector::PremonomorphicSentinel(GetIsolate()),
              SKIP_WRITE_BARRIER);
  SetFeedbackExtra(*FeedbackVector::UninitializedSentinel(GetIsolate()),
                   SKIP_WRITE_BARRIER);
}

bool FeedbackNexus::ConfigureMegamorphic(IcCheckType property_type) {
  DisallowHeapAllocation no_gc;
  Isolate* isolate = GetIsolate();
  bool changed = false;
  Symbol* sentinel = *FeedbackVector::MegamorphicSentinel(isolate);
  if (GetFeedback() != sentinel) {
    SetFeedback(sentinel, SKIP_WRITE_BARRIER);
    changed = true;
  }

  Smi* extra = Smi::FromInt(static_cast<int>(property_type));
  if (changed || GetFeedbackExtra() != MaybeObject::FromSmi(extra)) {
    SetFeedbackExtra(extra, SKIP_WRITE_BARRIER);
    changed = true;
  }
  return changed;
}

InlineCacheState FeedbackNexus::StateFromFeedback() const {
  Isolate* isolate = GetIsolate();
  Object* feedback = GetFeedback();

  switch (kind()) {
    case FeedbackSlotKind::kCreateClosure:
    case FeedbackSlotKind::kLiteral:
      // CreateClosure and literal slots don't have a notion of state.
      UNREACHABLE();
      break;

    case FeedbackSlotKind::kStoreGlobalSloppy:
    case FeedbackSlotKind::kStoreGlobalStrict:
    case FeedbackSlotKind::kLoadGlobalNotInsideTypeof:
    case FeedbackSlotKind::kLoadGlobalInsideTypeof: {
      if (feedback->IsSmi()) return MONOMORPHIC;

      MaybeObject* extra = GetFeedbackExtra();
      if (!WeakCell::cast(feedback)->cleared() ||
          extra != MaybeObject::FromObject(
                       *FeedbackVector::UninitializedSentinel(isolate))) {
        return MONOMORPHIC;
      }
      return UNINITIALIZED;
    }

    case FeedbackSlotKind::kStoreNamedSloppy:
    case FeedbackSlotKind::kStoreNamedStrict:
    case FeedbackSlotKind::kStoreKeyedSloppy:
    case FeedbackSlotKind::kStoreKeyedStrict:
    case FeedbackSlotKind::kStoreInArrayLiteral:
    case FeedbackSlotKind::kStoreOwnNamed:
    case FeedbackSlotKind::kLoadProperty:
    case FeedbackSlotKind::kLoadKeyed: {
      if (feedback == *FeedbackVector::UninitializedSentinel(isolate)) {
        return UNINITIALIZED;
      }
      if (feedback == *FeedbackVector::MegamorphicSentinel(isolate)) {
        return MEGAMORPHIC;
      }
      if (feedback == *FeedbackVector::PremonomorphicSentinel(isolate)) {
        return PREMONOMORPHIC;
      }
      if (feedback->IsWeakFixedArray()) {
        // Determine state purely by our structure, don't check if the maps are
        // cleared.
        return POLYMORPHIC;
      }
      if (feedback->IsWeakCell()) {
        // Don't check if the map is cleared.
        return MONOMORPHIC;
      }
      if (feedback->IsName()) {
        DCHECK(IsKeyedLoadICKind(kind()) || IsKeyedStoreICKind(kind()));
        Object* extra = GetFeedbackExtra()->ToStrongHeapObject();
        WeakFixedArray* extra_array = WeakFixedArray::cast(extra);
        return extra_array->length() > 2 ? POLYMORPHIC : MONOMORPHIC;
      }
      UNREACHABLE();
    }
    case FeedbackSlotKind::kCall: {
      if (feedback == *FeedbackVector::MegamorphicSentinel(isolate)) {
        return GENERIC;
      } else if (feedback->IsAllocationSite() || feedback->IsWeakCell()) {
        return MONOMORPHIC;
      }

      CHECK(feedback == *FeedbackVector::UninitializedSentinel(isolate));
      return UNINITIALIZED;
    }
    case FeedbackSlotKind::kBinaryOp: {
      BinaryOperationHint hint = GetBinaryOperationFeedback();
      if (hint == BinaryOperationHint::kNone) {
        return UNINITIALIZED;
      } else if (hint == BinaryOperationHint::kAny) {
        return GENERIC;
      }

      return MONOMORPHIC;
    }
    case FeedbackSlotKind::kCompareOp: {
      CompareOperationHint hint = GetCompareOperationFeedback();
      if (hint == CompareOperationHint::kNone) {
        return UNINITIALIZED;
      } else if (hint == CompareOperationHint::kAny) {
        return GENERIC;
      }

      return MONOMORPHIC;
    }
    case FeedbackSlotKind::kForIn: {
      ForInHint hint = GetForInFeedback();
      if (hint == ForInHint::kNone) {
        return UNINITIALIZED;
      } else if (hint == ForInHint::kAny) {
        return GENERIC;
      }
      return MONOMORPHIC;
    }
    case FeedbackSlotKind::kInstanceOf: {
      if (feedback == *FeedbackVector::UninitializedSentinel(isolate)) {
        return UNINITIALIZED;
      } else if (feedback == *FeedbackVector::MegamorphicSentinel(isolate)) {
        return MEGAMORPHIC;
      }
      return MONOMORPHIC;
    }
    case FeedbackSlotKind::kStoreDataPropertyInLiteral: {
      if (feedback == *FeedbackVector::UninitializedSentinel(isolate)) {
        return UNINITIALIZED;
      } else if (feedback->IsWeakCell()) {
        // Don't check if the map is cleared.
        return MONOMORPHIC;
      }

      return MEGAMORPHIC;
    }
    case FeedbackSlotKind::kTypeProfile: {
      if (feedback == *FeedbackVector::UninitializedSentinel(isolate)) {
        return UNINITIALIZED;
      }
      return MONOMORPHIC;
    }

    case FeedbackSlotKind::kInvalid:
    case FeedbackSlotKind::kKindsNumber:
      UNREACHABLE();
      break;
  }
  return UNINITIALIZED;
}

void FeedbackNexus::ConfigurePropertyCellMode(Handle<PropertyCell> cell) {
  DCHECK(IsGlobalICKind(kind()));
  Isolate* isolate = GetIsolate();
  SetFeedback(*isolate->factory()->NewWeakCell(cell));
  SetFeedbackExtra(*FeedbackVector::UninitializedSentinel(isolate),
                   SKIP_WRITE_BARRIER);
}

bool FeedbackNexus::ConfigureLexicalVarMode(int script_context_index,
                                            int context_slot_index) {
  DCHECK(IsGlobalICKind(kind()));
  DCHECK_LE(0, script_context_index);
  DCHECK_LE(0, context_slot_index);
  if (!ContextIndexBits::is_valid(script_context_index) ||
      !SlotIndexBits::is_valid(context_slot_index)) {
    return false;
  }
  int config = ContextIndexBits::encode(script_context_index) |
               SlotIndexBits::encode(context_slot_index);

  SetFeedback(Smi::FromInt(config));
  Isolate* isolate = GetIsolate();
  SetFeedbackExtra(*FeedbackVector::UninitializedSentinel(isolate),
                   SKIP_WRITE_BARRIER);
  return true;
}

void FeedbackNexus::ConfigureHandlerMode(Handle<Object> handler) {
  DCHECK(IsGlobalICKind(kind()));
  DCHECK(IC::IsHandler(*handler));
  SetFeedback(GetIsolate()->heap()->empty_weak_cell());
  if (handler->IsMap()) {
    SetFeedbackExtra(HeapObjectReference::Weak(HeapObject::cast(*handler)));
  } else {
    SetFeedbackExtra(*handler);
  }
}

int FeedbackNexus::GetCallCount() {
  DCHECK(IsCallICKind(kind()));

  Object* call_count = GetFeedbackExtra()->ToObject();
  CHECK(call_count->IsSmi());
  uint32_t value = static_cast<uint32_t>(Smi::ToInt(call_count));
  return CallCountField::decode(value);
}

void FeedbackNexus::SetSpeculationMode(SpeculationMode mode) {
  DCHECK(IsCallICKind(kind()));

  Object* call_count = GetFeedbackExtra()->ToObject();
  CHECK(call_count->IsSmi());
  uint32_t count = static_cast<uint32_t>(Smi::ToInt(call_count));
  uint32_t value = CallCountField::encode(CallCountField::decode(count));
  int result = static_cast<int>(value | SpeculationModeField::encode(mode));
  SetFeedbackExtra(Smi::FromInt(result), SKIP_WRITE_BARRIER);
}

SpeculationMode FeedbackNexus::GetSpeculationMode() {
  DCHECK(IsCallICKind(kind()));

  Object* call_count = GetFeedbackExtra()->ToObject();
  CHECK(call_count->IsSmi());
  uint32_t value = static_cast<uint32_t>(Smi::ToInt(call_count));
  return SpeculationModeField::decode(value);
}

float FeedbackNexus::ComputeCallFrequency() {
  DCHECK(IsCallICKind(kind()));

  double const invocation_count = vector()->invocation_count();
  double const call_count = GetCallCount();
  if (invocation_count == 0) {
    // Prevent division by 0.
    return 0.0f;
  }
  return static_cast<float>(call_count / invocation_count);
}

void FeedbackNexus::ConfigureMonomorphic(Handle<Name> name,
                                         Handle<Map> receiver_map,
                                         Handle<Object> handler) {
  DCHECK(handler.is_null() || IC::IsHandler(*handler));
  Handle<WeakCell> cell = Map::WeakCellForMap(receiver_map);
  if (kind() == FeedbackSlotKind::kStoreDataPropertyInLiteral) {
    SetFeedback(*cell);
    SetFeedbackExtra(*name);
  } else {
    if (name.is_null()) {
      SetFeedback(*cell);
      if (handler->IsMap()) {
        SetFeedbackExtra(HeapObjectReference::Weak(*handler));
      } else {
        SetFeedbackExtra(*handler);
      }
    } else {
      Handle<WeakFixedArray> array = EnsureExtraArrayOfSize(2);
      SetFeedback(*name);
      array->Set(0, HeapObjectReference::Strong(*cell));
      if (handler->IsMap()) {
        array->Set(1, HeapObjectReference::Weak(*handler));
      } else {
        array->Set(1, MaybeObject::FromObject(*handler));
      }
    }
  }
}

void FeedbackNexus::ConfigurePolymorphic(Handle<Name> name,
                                         MapHandles const& maps,
                                         ObjectHandles* handlers) {
  DCHECK_EQ(handlers->size(), maps.size());
  int receiver_count = static_cast<int>(maps.size());
  DCHECK_GT(receiver_count, 1);
  Handle<WeakFixedArray> array;
  if (name.is_null()) {
    array = EnsureArrayOfSize(receiver_count * 2);
    SetFeedbackExtra(*FeedbackVector::UninitializedSentinel(GetIsolate()),
                     SKIP_WRITE_BARRIER);
  } else {
    array = EnsureExtraArrayOfSize(receiver_count * 2);
    SetFeedback(*name);
  }

  for (int current = 0; current < receiver_count; ++current) {
    Handle<Map> map = maps[current];
    Handle<WeakCell> cell = Map::WeakCellForMap(map);
    array->Set(current * 2, HeapObjectReference::Strong(*cell));
    DCHECK(IC::IsHandler(*handlers->at(current)));
    if (handlers->at(current)->IsMap()) {
      array->Set(current * 2 + 1,
                 HeapObjectReference::Weak(*handlers->at(current)));
    } else {
      array->Set(current * 2 + 1,
                 MaybeObject::FromObject(*handlers->at(current)));
    }
  }
}

int FeedbackNexus::ExtractMaps(MapHandles* maps) const {
  DCHECK(IsLoadICKind(kind()) || IsStoreICKind(kind()) ||
         IsKeyedLoadICKind(kind()) || IsKeyedStoreICKind(kind()) ||
         IsStoreOwnICKind(kind()) || IsStoreDataPropertyInLiteralKind(kind()) ||
         IsStoreInArrayLiteralICKind(kind()));

  Isolate* isolate = GetIsolate();
  Object* feedback = GetFeedback();
  bool is_named_feedback = IsPropertyNameFeedback(feedback);
  if (feedback->IsWeakFixedArray() || is_named_feedback) {
    int found = 0;
    WeakFixedArray* array;
    if (is_named_feedback) {
      array = WeakFixedArray::cast(GetFeedbackExtra()->ToStrongHeapObject());
    } else {
      array = WeakFixedArray::cast(feedback);
    }
    const int increment = 2;
    for (int i = 0; i < array->length(); i += increment) {
      WeakCell* cell = WeakCell::cast(array->Get(i)->ToStrongHeapObject());
      if (!cell->cleared()) {
        Map* map = Map::cast(cell->value());
        maps->push_back(handle(map, isolate));
        found++;
      }
    }
    return found;
  } else if (feedback->IsWeakCell()) {
    WeakCell* cell = WeakCell::cast(feedback);
    if (!cell->cleared()) {
      Map* map = Map::cast(cell->value());
      maps->push_back(handle(map, isolate));
      return 1;
    }
  }

  return 0;
}

MaybeHandle<Object> FeedbackNexus::FindHandlerForMap(Handle<Map> map) const {
  DCHECK(IsLoadICKind(kind()) || IsStoreICKind(kind()) ||
         IsKeyedLoadICKind(kind()) || IsKeyedStoreICKind(kind()) ||
         IsStoreOwnICKind(kind()) || IsStoreDataPropertyInLiteralKind(kind()));

  Object* feedback = GetFeedback();
  Isolate* isolate = GetIsolate();
  bool is_named_feedback = IsPropertyNameFeedback(feedback);
  if (feedback->IsWeakFixedArray() || is_named_feedback) {
    WeakFixedArray* array;
    if (is_named_feedback) {
      array = WeakFixedArray::cast(GetFeedbackExtra()->ToStrongHeapObject());
    } else {
      array = WeakFixedArray::cast(feedback);
    }
    const int increment = 2;
    for (int i = 0; i < array->length(); i += increment) {
      WeakCell* cell = WeakCell::cast(array->Get(i)->ToStrongHeapObject());
      if (!cell->cleared()) {
        Map* array_map = Map::cast(cell->value());
        if (array_map == *map &&
            !array->Get(i + increment - 1)->IsClearedWeakHeapObject()) {
          // This converts a weak reference to a strong reference.
          Object* handler = array->Get(i + increment - 1)->GetHeapObjectOrSmi();
          DCHECK(IC::IsHandler(handler));
          return handle(handler, isolate);
        }
      }
    }
  } else if (feedback->IsWeakCell()) {
    WeakCell* cell = WeakCell::cast(feedback);
    if (!cell->cleared()) {
      Map* cell_map = Map::cast(cell->value());
      if (cell_map == *map && !GetFeedbackExtra()->IsClearedWeakHeapObject()) {
        // This converts a weak reference to a strong reference.
        Object* handler = GetFeedbackExtra()->GetHeapObjectOrSmi();
        DCHECK(IC::IsHandler(handler));
        return handle(handler, isolate);
      }
    }
  }

  return MaybeHandle<Code>();
}

bool FeedbackNexus::FindHandlers(ObjectHandles* code_list, int length) const {
  DCHECK(IsLoadICKind(kind()) || IsStoreICKind(kind()) ||
         IsKeyedLoadICKind(kind()) || IsKeyedStoreICKind(kind()) ||
         IsStoreOwnICKind(kind()) || IsStoreDataPropertyInLiteralKind(kind()) ||
         IsStoreInArrayLiteralICKind(kind()));

  Object* feedback = GetFeedback();
  Isolate* isolate = GetIsolate();
  int count = 0;
  bool is_named_feedback = IsPropertyNameFeedback(feedback);
  if (feedback->IsWeakFixedArray() || is_named_feedback) {
    WeakFixedArray* array;
    if (is_named_feedback) {
      array = WeakFixedArray::cast(GetFeedbackExtra()->ToStrongHeapObject());
    } else {
      array = WeakFixedArray::cast(feedback);
    }
    const int increment = 2;
    for (int i = 0; i < array->length(); i += increment) {
      WeakCell* cell = WeakCell::cast(array->Get(i)->ToStrongHeapObject());
      // Be sure to skip handlers whose maps have been cleared.
      if (!cell->cleared() &&
          !array->Get(i + increment - 1)->IsClearedWeakHeapObject()) {
        // This converts a weak reference to a strong reference.
        Object* handler = array->Get(i + increment - 1)->GetHeapObjectOrSmi();
        DCHECK(IC::IsHandler(handler));
        code_list->push_back(handle(handler, isolate));
        count++;
      }
    }
  } else if (feedback->IsWeakCell()) {
    WeakCell* cell = WeakCell::cast(feedback);
    MaybeObject* extra = GetFeedbackExtra();
    if (!cell->cleared() && !extra->IsClearedWeakHeapObject()) {
      // This converts a weak reference to a strong reference.
      Object* handler = extra->GetHeapObjectOrSmi();
      DCHECK(IC::IsHandler(handler));
      code_list->push_back(handle(handler, isolate));
      count++;
    }
  }
  return count == length;
}

Name* FeedbackNexus::FindFirstName() const {
  if (IsKeyedStoreICKind(kind()) || IsKeyedLoadICKind(kind())) {
    Object* feedback = GetFeedback();
    if (IsPropertyNameFeedback(feedback)) {
      return Name::cast(feedback);
    }
  }
  return nullptr;
}

KeyedAccessLoadMode FeedbackNexus::GetKeyedAccessLoadMode() const {
  DCHECK(IsKeyedLoadICKind(kind()));
  MapHandles maps;
  ObjectHandles handlers;

  if (GetKeyType() == PROPERTY) return STANDARD_LOAD;

  ExtractMaps(&maps);
  FindHandlers(&handlers, static_cast<int>(maps.size()));
  for (Handle<Object> const& handler : handlers) {
    KeyedAccessLoadMode mode = LoadHandler::GetKeyedAccessLoadMode(*handler);
    if (mode != STANDARD_LOAD) return mode;
  }

  return STANDARD_LOAD;
}

KeyedAccessStoreMode FeedbackNexus::GetKeyedAccessStoreMode() const {
  DCHECK(IsKeyedStoreICKind(kind()) || IsStoreInArrayLiteralICKind(kind()));
  KeyedAccessStoreMode mode = STANDARD_STORE;
  MapHandles maps;
  ObjectHandles handlers;

  if (GetKeyType() == PROPERTY) return mode;

  ExtractMaps(&maps);
  FindHandlers(&handlers, static_cast<int>(maps.size()));
  for (const Handle<Object>& maybe_code_handler : handlers) {
    // The first handler that isn't the slow handler will have the bits we need.
    Handle<Code> handler;
    if (maybe_code_handler->IsStoreHandler()) {
      Handle<StoreHandler> data_handler =
          Handle<StoreHandler>::cast(maybe_code_handler);
      handler = handle(Code::cast(data_handler->smi_handler()));
    } else if (maybe_code_handler->IsSmi()) {
      // Skip proxy handlers.
      DCHECK_EQ(*maybe_code_handler, *StoreHandler::StoreProxy(GetIsolate()));
      continue;
    } else {
      // Element store without prototype chain check.
      handler = Handle<Code>::cast(maybe_code_handler);
      if (handler->is_builtin()) continue;
    }
    CodeStub::Major major_key = CodeStub::MajorKeyFromKey(handler->stub_key());
    uint32_t minor_key = CodeStub::MinorKeyFromKey(handler->stub_key());
    CHECK(major_key == CodeStub::KeyedStoreSloppyArguments ||
          major_key == CodeStub::StoreFastElement ||
          major_key == CodeStub::StoreSlowElement ||
          major_key == CodeStub::StoreInArrayLiteralSlow ||
          major_key == CodeStub::ElementsTransitionAndStore ||
          major_key == CodeStub::NoCache);
    if (major_key != CodeStub::NoCache) {
      mode = CommonStoreModeBits::decode(minor_key);
      break;
    }
  }

  return mode;
}

IcCheckType FeedbackNexus::GetKeyType() const {
  DCHECK(IsKeyedStoreICKind(kind()) || IsKeyedLoadICKind(kind()) ||
         IsStoreInArrayLiteralICKind(kind()));
  Object* feedback = GetFeedback();
  if (feedback == *FeedbackVector::MegamorphicSentinel(GetIsolate())) {
    return static_cast<IcCheckType>(Smi::ToInt(GetFeedbackExtra()->ToObject()));
  }
  return IsPropertyNameFeedback(feedback) ? PROPERTY : ELEMENT;
}

BinaryOperationHint FeedbackNexus::GetBinaryOperationFeedback() const {
  DCHECK_EQ(kind(), FeedbackSlotKind::kBinaryOp);
  int feedback = Smi::ToInt(GetFeedback());
  return BinaryOperationHintFromFeedback(feedback);
}

CompareOperationHint FeedbackNexus::GetCompareOperationFeedback() const {
  DCHECK_EQ(kind(), FeedbackSlotKind::kCompareOp);
  int feedback = Smi::ToInt(GetFeedback());
  return CompareOperationHintFromFeedback(feedback);
}

ForInHint FeedbackNexus::GetForInFeedback() const {
  DCHECK_EQ(kind(), FeedbackSlotKind::kForIn);
  int feedback = Smi::ToInt(GetFeedback());
  return ForInHintFromFeedback(feedback);
}

Handle<FeedbackCell> FeedbackNexus::GetFeedbackCell() const {
  DCHECK_EQ(FeedbackSlotKind::kCreateClosure, kind());
  return handle(FeedbackCell::cast(GetFeedback()));
}

MaybeHandle<JSObject> FeedbackNexus::GetConstructorFeedback() const {
  DCHECK_EQ(kind(), FeedbackSlotKind::kInstanceOf);
  Isolate* isolate = GetIsolate();
  Object* feedback = GetFeedback();
  if (feedback->IsWeakCell() && !WeakCell::cast(feedback)->cleared()) {
    return handle(JSObject::cast(WeakCell::cast(feedback)->value()), isolate);
  }
  return MaybeHandle<JSObject>();
}

namespace {

bool InList(Handle<ArrayList> types, Handle<String> type) {
  for (int i = 0; i < types->Length(); i++) {
    Object* obj = types->Get(i);
    if (String::cast(obj)->Equals(*type)) {
      return true;
    }
  }
  return false;
}
}  // anonymous namespace

void FeedbackNexus::Collect(Handle<String> type, int position) {
  DCHECK(IsTypeProfileKind(kind()));
  DCHECK_GE(position, 0);
  Isolate* isolate = GetIsolate();

  Object* const feedback = GetFeedback();

  // Map source position to collection of types
  Handle<SimpleNumberDictionary> types;

  if (feedback == *FeedbackVector::UninitializedSentinel(isolate)) {
    types = SimpleNumberDictionary::New(isolate, 1);
  } else {
    types = handle(SimpleNumberDictionary::cast(feedback));
  }

  Handle<ArrayList> position_specific_types;

  int entry = types->FindEntry(position);
  if (entry == SimpleNumberDictionary::kNotFound) {
    position_specific_types = ArrayList::New(isolate, 1);
    types = SimpleNumberDictionary::Set(
        types, position, ArrayList::Add(position_specific_types, type));
  } else {
    DCHECK(types->ValueAt(entry)->IsArrayList());
    position_specific_types = handle(ArrayList::cast(types->ValueAt(entry)));
    if (!InList(position_specific_types, type)) {  // Add type
      types = SimpleNumberDictionary::Set(
          types, position, ArrayList::Add(position_specific_types, type));
    }
  }
  SetFeedback(*types);
}

std::vector<int> FeedbackNexus::GetSourcePositions() const {
  DCHECK(IsTypeProfileKind(kind()));
  std::vector<int> source_positions;
  Isolate* isolate = GetIsolate();

  Object* const feedback = GetFeedback();

  if (feedback == *FeedbackVector::UninitializedSentinel(isolate)) {
    return source_positions;
  }

  Handle<SimpleNumberDictionary> types = Handle<SimpleNumberDictionary>(
      SimpleNumberDictionary::cast(feedback), isolate);

  for (int index = SimpleNumberDictionary::kElementsStartIndex;
       index < types->length(); index += SimpleNumberDictionary::kEntrySize) {
    int key_index = index + SimpleNumberDictionary::kEntryKeyIndex;
    Object* key = types->get(key_index);
    if (key->IsSmi()) {
      int position = Smi::cast(key)->value();
      source_positions.push_back(position);
    }
  }
  return source_positions;
}

std::vector<Handle<String>> FeedbackNexus::GetTypesForSourcePositions(
    uint32_t position) const {
  DCHECK(IsTypeProfileKind(kind()));
  Isolate* isolate = GetIsolate();

  Object* const feedback = GetFeedback();
  std::vector<Handle<String>> types_for_position;
  if (feedback == *FeedbackVector::UninitializedSentinel(isolate)) {
    return types_for_position;
  }

  Handle<SimpleNumberDictionary> types = Handle<SimpleNumberDictionary>(
      SimpleNumberDictionary::cast(feedback), isolate);

  int entry = types->FindEntry(position);
  if (entry == SimpleNumberDictionary::kNotFound) {
    return types_for_position;
  }
  DCHECK(types->ValueAt(entry)->IsArrayList());
  Handle<ArrayList> position_specific_types =
      Handle<ArrayList>(ArrayList::cast(types->ValueAt(entry)));
  for (int i = 0; i < position_specific_types->Length(); i++) {
    Object* t = position_specific_types->Get(i);
    types_for_position.push_back(Handle<String>(String::cast(t), isolate));
  }

  return types_for_position;
}

namespace {

Handle<JSObject> ConvertToJSObject(Isolate* isolate,
                                   Handle<SimpleNumberDictionary> feedback) {
  Handle<JSObject> type_profile =
      isolate->factory()->NewJSObject(isolate->object_function());

  for (int index = SimpleNumberDictionary::kElementsStartIndex;
       index < feedback->length();
       index += SimpleNumberDictionary::kEntrySize) {
    int key_index = index + SimpleNumberDictionary::kEntryKeyIndex;
    Object* key = feedback->get(key_index);
    if (key->IsSmi()) {
      int value_index = index + SimpleNumberDictionary::kEntryValueIndex;

      Handle<ArrayList> position_specific_types(
          ArrayList::cast(feedback->get(value_index)));

      int position = Smi::ToInt(key);
      JSObject::AddDataElement(
          type_profile, position,
          isolate->factory()->NewJSArrayWithElements(
              ArrayList::Elements(position_specific_types)),
          PropertyAttributes::NONE)
          .ToHandleChecked();
    }
  }
  return type_profile;
}
}  // namespace

JSObject* FeedbackNexus::GetTypeProfile() const {
  DCHECK(IsTypeProfileKind(kind()));
  Isolate* isolate = GetIsolate();

  Object* const feedback = GetFeedback();

  if (feedback == *FeedbackVector::UninitializedSentinel(isolate)) {
    return *isolate->factory()->NewJSObject(isolate->object_function());
  }

  return *ConvertToJSObject(isolate,
                            handle(SimpleNumberDictionary::cast(feedback)));
}

void FeedbackNexus::ResetTypeProfile() {
  DCHECK(IsTypeProfileKind(kind()));
  SetFeedback(*FeedbackVector::UninitializedSentinel(GetIsolate()));
}

}  // namespace internal
}  // namespace v8
